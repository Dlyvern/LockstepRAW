#include "FramePasses.hpp"

#include "Lockstep/Core/Log.hpp"

#include "FrameAttachments.hpp"
#include "RendererState.hpp"
#include "ShaderLoader.hpp"
#include "VelaConversions.hpp"

#include <Vela/Graphics/VertexLayout.hpp>

#include <algorithm>
#include <bit>
#include <cstddef>
#include <span>
#include <string>
#include <string_view>
#include <utility>

namespace lockstep::render
{
    namespace
    {
        constexpr std::string_view k_fullscreenVertexShader{"fullscreen.vert.spv"};
        constexpr std::string_view k_environmentFragmentShader{"environment.frag.spv"};
        constexpr std::string_view k_skyVertexShader{"sky.vert.spv"};
        constexpr std::string_view k_skyFragmentShader{"sky.frag.spv"};
        constexpr uint32_t k_lightingSlot{0};
        constexpr uint32_t k_shadowAtlasSlot{0};
        constexpr uint32_t k_environmentSlot{1};
        constexpr uint32_t k_planarReflectionSlot{3};
        constexpr uint32_t k_fullscreenTriangleVertices{3};
        constexpr float k_decalBlend{2.0f};

        struct PbrDrawConstants
        {
            math::Mat4 model;
            math::Mat4 previousModel;
            PbrFactors factors;
        };

        static_assert(sizeof(PbrDrawConstants) == 224);

        vela::graphics::AttachmentSlot clearedSlot(std::string_view name, vela::graphics::TextureFormat format)
        {
            vela::graphics::AttachmentSlot slot{};
            slot.name = std::string(name);
            slot.format = format;
            slot.load = vela::graphics::LoadOp::Clear;
            slot.store = vela::graphics::StoreOp::Store;

            return slot;
        }

        void multisample(vela::graphics::AttachmentSlot& slot, std::string_view multisampledName, uint32_t samples)
        {
            slot.resolveTarget = slot.name;
            slot.name = std::string(multisampledName);
            slot.samples = samples;
        }

        vela::graphics::RenderState fullscreenState()
        {
            vela::graphics::RenderState renderState{};
            renderState.depthTest = false;
            renderState.depthWrite = false;

            return renderState;
        }
    } //namespace

    vela::graphics::SamplerDescription shadowCompareSampler()
    {
        vela::graphics::SamplerDescription sampler{};
        sampler.addressU = vela::graphics::AddressMode::ClampToEdge;
        sampler.addressV = vela::graphics::AddressMode::ClampToEdge;
        sampler.addressW = vela::graphics::AddressMode::ClampToEdge;
        sampler.mipMode = vela::graphics::MipMode::Nearest;
        sampler.maxLod = 0.0f;
        sampler.compareOp = vela::graphics::DepthCompare::LessOrEqual;

        return sampler;
    }

    vela::graphics::SamplerDescription shadowDepthSampler()
    {
        vela::graphics::SamplerDescription sampler = shadowCompareSampler();
        sampler.minFilter = vela::graphics::Filter::Nearest;
        sampler.magFilter = vela::graphics::Filter::Nearest;
        sampler.compareOp = vela::graphics::DepthCompare::None;

        return sampler;
    }

    void recordSceneDraw(vela::graphics::PassRecorder& recorder, const DrawItem& item, PbrView view)
    {
        if (item.material->shading == Shading::Unlit)
        {
            recorder.draw(*item.mesh, item.material->material, item.model);
            return;
        }

        PbrDrawConstants constants{.model = item.model, .previousModel = item.previousModel,
            .factors = item.material->factors};
        constants.factors.flags[2] = static_cast<float>(view);

        if (item.isDecal && view == PbrView::Camera && item.material->alphaMode == AlphaMode::Blend)
        {
            constants.factors.flags[1] = k_decalBlend;
        }

        constants.factors.flags[3] = static_cast<float>(item.jointBase);
        constants.factors.detail[3] = std::bit_cast<float>(item.lightMask);
        std::copy(item.shading.begin(), item.shading.end(), constants.factors.shading);

        for (uint32_t channel = 0; channel < 3; ++channel)
        {
            constants.factors.emissiveNormalScale[channel] *= item.emissiveScale;
        }

        recorder.draw(*item.mesh, item.material->material, constants);
    }

    Result<vela::graphics::Material> createBuiltinMaterial(vela::core::Context& context, std::string_view vertexFile,
        std::string_view fragmentFile, const vela::graphics::VertexLayout& layout,
        const vela::graphics::RenderState& renderState)
    {
        auto vertexShader = loadShader(vertexFile);

        if (!vertexShader)
        {
            return vertexShader.error();
        }

        auto fragmentShader = loadShader(fragmentFile);

        if (!fragmentShader)
        {
            return fragmentShader.error();
        }

        auto material = vela::graphics::Material::create(context,
        {
            .vertexShader = vertexShader.value(),
            .fragmentShader = fragmentShader.value(),
            .vertexLayout = layout,
            .renderState = renderState
        });

        if (!material)
        {
            return toError(material.error());
        }

        return std::move(material).value();
    }

    Result<std::unique_ptr<EnvironmentPass>> EnvironmentPass::create(RendererState& state)
    {
        auto material = createBuiltinMaterial(state.context, k_fullscreenVertexShader, k_environmentFragmentShader,
            vela::graphics::VertexLayout{}, fullscreenState());

        if (!material)
        {
            return material.error();
        }

        if (auto bound = material.value().setStorageBuffer(k_lightingSlot, state.lightBuffer); !bound)
        {
            return toError(bound.error());
        }

        if (auto bound = material.value().setTexture(k_cloudNoiseSlot, *state.atmosphere.cloudNoise); !bound)
        {
            return toError(bound.error());
        }

        return std::unique_ptr<EnvironmentPass>(new EnvironmentPass(state, std::move(material).value()));
    }

    EnvironmentPass::EnvironmentPass(RendererState& state, vela::graphics::Material&& material)
        : m_state(&state),
          m_material(std::move(material))
    {
    }

    vela::graphics::PassDescription EnvironmentPass::describe() const
    {
        vela::graphics::AttachmentSlot atlas = clearedSlot(k_environmentAttachment, k_sceneColorFormat);
        atlas.size = {k_environmentTileSize * k_environmentTiles, k_environmentTileSize};
        atlas.clear = {0.0f, 0.0f, 0.0f, 1.0f};

        vela::graphics::PassDescription description{};
        description.colorOutputs.push_back(atlas);

        return description;
    }

    void EnvironmentPass::record(vela::graphics::PassRecorder& recorder)
    {
        const GpuFrameLighting& lighting = m_state->prepared.lighting;

        if (auto uploaded = m_state->lightBuffer.update(std::as_bytes(std::span(&lighting, 1))); !uploaded)
        {
            log(LogLevel::Error, "Render", "lighting upload failed: " + uploaded.error().message);
            return;
        }

        if (lighting.iblParams[3] <= 0.0f)
        {
            return;
        }

        recorder.bind(m_material);
        recorder.drawInstanced(k_fullscreenTriangleVertices, 1);
    }

    Result<std::unique_ptr<LitScenePass>> LitScenePass::create(RendererState& state)
    {
        vela::graphics::RenderState skyState{};
        skyState.depthWrite = false;
        skyState.depthCompare = vela::graphics::DepthCompare::LessOrEqual;

        auto material = createBuiltinMaterial(state.context, k_skyVertexShader, k_skyFragmentShader,
            vela::graphics::VertexLayout{}, skyState);

        if (!material)
        {
            return material.error();
        }

        if (auto bound = material.value().setStorageBuffer(k_lightingSlot, state.lightBuffer); !bound)
        {
            return toError(bound.error());
        }

        if (auto bound = material.value().setTexture(k_cloudNoiseSlot, *state.atmosphere.cloudNoise); !bound)
        {
            return toError(bound.error());
        }

        return std::unique_ptr<LitScenePass>(new LitScenePass(state, std::move(material).value()));
    }

    LitScenePass::LitScenePass(const RendererState& state, vela::graphics::Material&& skyMaterial)
        : m_state(&state),
          m_skyMaterial(std::move(skyMaterial))
    {
    }

    vela::graphics::PassDescription LitScenePass::describe() const
    {
        vela::graphics::AttachmentSlot color = clearedSlot(k_sceneColorAttachment, k_sceneColorFormat);
        color.clear = {0.0f, 0.0f, 0.0f, 1.0f};
        vela::graphics::AttachmentSlot ambient = clearedSlot(k_sceneAmbientLitAttachment, k_sceneColorFormat);
        ambient.clear = {0.0f, 0.0f, 0.0f, 0.0f};
        vela::graphics::AttachmentSlot surfaceNormal = clearedSlot(k_surfaceNormalAttachment, k_sceneColorFormat);
        surfaceNormal.clear = {0.0f, 0.0f, 1.0f, 1.0f};
        vela::graphics::AttachmentSlot surfaceSpecular = clearedSlot(k_surfaceSpecularAttachment, k_sceneColorFormat);
        surfaceSpecular.clear = {0.0f, 0.0f, 0.0f, 1.0f};

        vela::graphics::AttachmentSlot velocity = clearedSlot(k_sceneVelocityAttachment, k_sceneColorFormat);
        velocity.clear = {0.0f, 0.0f, 0.0f, 0.0f};
        vela::graphics::AttachmentSlot depth = clearedSlot(k_sceneDepthAttachment, k_depthFormat);

        if (m_state->sceneSamples > 1)
        {
            multisample(color, k_sceneColorMsaaAttachment, m_state->sceneSamples);
            multisample(ambient, k_sceneAmbientMsaaAttachment, m_state->sceneSamples);
            multisample(velocity, k_sceneVelocityMsaaAttachment, m_state->sceneSamples);
            multisample(depth, k_sceneDepthMsaaAttachment, m_state->sceneSamples);
            multisample(surfaceNormal, k_surfaceNormalMsaaAttachment, m_state->sceneSamples);
            multisample(surfaceSpecular, k_surfaceSpecularMsaaAttachment, m_state->sceneSamples);
        }

        vela::graphics::PassDescription description{};
        description.colorOutputs.push_back(color);
        description.colorOutputs.push_back(ambient);
        description.colorOutputs.push_back(velocity);
        description.colorOutputs.push_back(surfaceNormal);
        description.colorOutputs.push_back(surfaceSpecular);
        description.depthOutput = depth;
        description.inputs.push_back({std::string(k_shadowMapAttachment), vela::graphics::InputUsage::Sampled});
        description.inputs.push_back({std::string(k_environmentAttachment), vela::graphics::InputUsage::Sampled});
        description.inputs.push_back({std::string(k_reflectionColorAttachment), vela::graphics::InputUsage::Sampled});
        description.inputs.push_back({std::string(k_scopeColorAttachment), vela::graphics::InputUsage::Sampled});

        return description;
    }

    void LitScenePass::record(vela::graphics::PassRecorder& recorder)
    {
        recorder.bindAttachment(k_shadowAtlasSlot, std::string(k_shadowMapAttachment), shadowCompareSampler());
        recorder.bindAttachment(k_environmentSlot, std::string(k_environmentAttachment));
        recorder.bindAttachment(k_shadowDepthSlot, std::string(k_shadowMapAttachment), shadowDepthSampler());
        recorder.bindAttachment(k_planarReflectionSlot, std::string(k_reflectionColorAttachment));

        auto drawList = [this, &recorder](const std::vector<uint32_t>& list)
        {
            for (const uint32_t index : list)
            {
                const DrawItem& item = m_state->draws[index];
                recordSceneDraw(recorder, item, item.isReflector ? PbrView::ReflectorSurface : PbrView::Camera);
            }
        };

        auto viewModelList = [this, &recorder](const std::vector<uint32_t>& list)
        {
            for (const uint32_t index : list)
            {
                const DrawItem& item = m_state->draws[index];

                if (item.isSightWindow)
                {
                    recordSceneDraw(recorder, item, PbrView::SightWindow);
                    continue;
                }

                if (!item.isScopeLens || !m_state->scope.active)
                {
                    recordSceneDraw(recorder, item, PbrView::ViewModel);
                    continue;
                }

                recorder.bindAttachment(k_planarReflectionSlot, std::string(k_scopeColorAttachment));
                recordSceneDraw(recorder, item, PbrView::ScopeLens);
                recorder.bindAttachment(k_planarReflectionSlot, std::string(k_reflectionColorAttachment));
            }
        };

        viewModelList(m_state->drawLists.viewModelOpaque);
        drawList(m_state->drawLists.opaque);
        drawList(m_state->drawLists.decals);

        if (m_state->prepared.lighting.skyGround[3] > 0.0f)
        {
            recorder.bind(m_skyMaterial);
            recorder.drawInstanced(k_fullscreenTriangleVertices, 1);
        }

        drawList(m_state->drawLists.blended);
        viewModelList(m_state->drawLists.viewModelBlended);
    }
} //namespace lockstep::render
