#include "FramePasses.hpp"

#include "FrameAttachments.hpp"
#include "RendererState.hpp"
#include "ShaderLoader.hpp"
#include "VelaConversions.hpp"

#include <Vela/Graphics/VertexLayout.hpp>

#include <cstddef>
#include <iostream>
#include <span>
#include <string>
#include <string_view>
#include <utility>

namespace lockstep::render
{
    namespace
    {
        constexpr std::string_view k_shadowVertexShader{"shadow.vert.spv"};
        constexpr std::string_view k_shadowFragmentShader{"shadow.frag.spv"};
        constexpr std::string_view k_fullscreenVertexShader{"fullscreen.vert.spv"};
        constexpr std::string_view k_tonemapFragmentShader{"tonemap.frag.spv"};
        constexpr uint32_t k_shadowMapSlot{0};
        constexpr uint32_t k_sceneColorSlot{0};
        constexpr uint32_t k_fullscreenTriangleVertices{3};
        constexpr float k_ditherAmplitude{1.0f / 255.0f};

        struct LitDrawConstants
        {
            math::Mat4 model;
            float surface[4];
        };

        struct TonemapConstants
        {
            float exposure;
            uint32_t tonemapper;
            float dither;
            uint32_t frameIndex;
        };

        static_assert(sizeof(LitDrawConstants) == 80);
        static_assert(sizeof(TonemapConstants) == 16);

        vela::graphics::VertexLayout positionOnlyLayout()
        {
            return vela::graphics::VertexLayout
            {
                sizeof(Vertex),
                {
                    {0, vela::graphics::VertexAttributeFormat::Float3, static_cast<uint32_t>(offsetof(Vertex, position))}
                }
            };
        }

        Result<vela::graphics::Material> createMaterial(vela::core::Context& context, std::string_view vertexFile,
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

        vela::graphics::AttachmentSlot clearedSlot(std::string_view name, vela::graphics::TextureFormat format)
        {
            vela::graphics::AttachmentSlot slot{};
            slot.name = std::string(name);
            slot.format = format;
            slot.load = vela::graphics::LoadOp::Clear;
            slot.store = vela::graphics::StoreOp::Store;

            return slot;
        }
    } //namespace

    Result<std::unique_ptr<ShadowPass>> ShadowPass::create(RendererState& state)
    {
        auto material = createMaterial(state.context, k_shadowVertexShader, k_shadowFragmentShader,
            positionOnlyLayout(), vela::graphics::RenderState{});

        if (!material)
        {
            return material.error();
        }

        return std::unique_ptr<ShadowPass>(new ShadowPass(state, std::move(material).value()));
    }

    ShadowPass::ShadowPass(const RendererState& state, vela::graphics::Material&& material)
        : m_state(&state),
          m_material(std::move(material))
    {
    }

    vela::graphics::PassDescription ShadowPass::describe() const
    {
        vela::graphics::AttachmentSlot depth = clearedSlot(k_shadowMapAttachment, k_depthFormat);
        depth.size = {k_shadowMapSize, k_shadowMapSize};
        depth.clear.depth = 1.0f;

        vela::graphics::PassDescription description{};
        description.depthOutput = depth;

        return description;
    }

    void ShadowPass::record(vela::graphics::PassRecorder& recorder)
    {
        if (m_state->prepared.caster == ShadowCaster::None)
        {
            return;
        }

        const math::Mat4& lightViewProjection = m_state->prepared.shadowViewProjection;

        for (const DrawItem& item : m_state->draws)
        {
            if (item.material->shading == Shading::Lit)
            {
                recorder.draw(*item.mesh, m_material, lightViewProjection * item.model);
            }
        }
    }

    LitScenePass::LitScenePass(RendererState& state)
        : m_state(&state)
    {
    }

    vela::graphics::PassDescription LitScenePass::describe() const
    {
        vela::graphics::AttachmentSlot color = clearedSlot(k_sceneColorAttachment, k_sceneColorFormat);
        color.clear = {0.0f, 0.0f, 0.0f, 1.0f};

        vela::graphics::PassDescription description{};
        description.colorOutputs.push_back(color);
        description.depthOutput = clearedSlot(k_sceneDepthAttachment, k_depthFormat);
        description.inputs.push_back({std::string(k_shadowMapAttachment), vela::graphics::InputUsage::Sampled});

        return description;
    }

    void LitScenePass::record(vela::graphics::PassRecorder& recorder)
    {
        const GpuFrameLighting& lighting = m_state->prepared.lighting;

        if (auto uploaded = m_state->lightBuffer.update(std::as_bytes(std::span(&lighting, 1))); !uploaded)
        {
            std::cerr << "Renderer: lighting upload failed: " << uploaded.error().message << '\n';
            return;
        }

        recorder.bindAttachment(k_shadowMapSlot, std::string(k_shadowMapAttachment));

        for (const DrawItem& item : m_state->draws)
        {
            if (item.material->shading == Shading::Unlit)
            {
                recorder.draw(*item.mesh, item.material->material, item.model);
                continue;
            }

            LitDrawConstants constants{.model = item.model, .surface = {}};

            for (std::size_t index = 0; index < 4; ++index)
            {
                constants.surface[index] = item.material->surface[index];
            }

            recorder.draw(*item.mesh, item.material->material, constants);
        }
    }

    Result<std::unique_ptr<TonemapPass>> TonemapPass::create(RendererState& state)
    {
        vela::graphics::RenderState renderState{};
        renderState.depthTest = false;
        renderState.depthWrite = false;

        auto material = createMaterial(state.context, k_fullscreenVertexShader, k_tonemapFragmentShader,
            vela::graphics::VertexLayout{}, renderState);

        if (!material)
        {
            return material.error();
        }

        return std::unique_ptr<TonemapPass>(new TonemapPass(state, std::move(material).value()));
    }

    TonemapPass::TonemapPass(const RendererState& state, vela::graphics::Material&& material)
        : m_state(&state),
          m_material(std::move(material))
    {
    }

    vela::graphics::PassDescription TonemapPass::describe() const
    {
        vela::graphics::AttachmentSlot display = clearedSlot(k_displayAttachment, k_displayFormat);
        display.load = vela::graphics::LoadOp::DontCare;

        vela::graphics::PassDescription description{};
        description.colorOutputs.push_back(display);
        description.inputs.push_back({std::string(k_sceneColorAttachment), vela::graphics::InputUsage::Sampled});

        return description;
    }

    void TonemapPass::record(vela::graphics::PassRecorder& recorder)
    {
        const TonemapSettings& settings = m_state->settings.tonemap;

        const TonemapConstants constants{
            .exposure = settings.enabled ? settings.exposure : 1.0f,
            .tonemapper = static_cast<uint32_t>(settings.enabled ? settings.tonemapper : Tonemapper::Clamp),
            .dither = settings.dither ? k_ditherAmplitude : 0.0f,
            .frameIndex = m_state->frameIndex};

        recorder.bind(m_material);
        recorder.setConstants(constants);
        recorder.bindAttachment(k_sceneColorSlot, std::string(k_sceneColorAttachment));
        recorder.drawInstanced(k_fullscreenTriangleVertices, 1);
    }
} //namespace lockstep::render
