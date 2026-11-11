#include "PostPasses.hpp"

#include "ExposurePasses.hpp"
#include "FrameAttachments.hpp"
#include "FramePasses.hpp"
#include "LensFlare.hpp"
#include "LensPasses.hpp"
#include "RenderScale.hpp"
#include "RendererState.hpp"
#include "TemporalAntiAliasing.hpp"
#include "VelaConversions.hpp"

#include <Vela/Graphics/RenderGraph.hpp>
#include <Vela/Graphics/VertexLayout.hpp>

#include <algorithm>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>

namespace lockstep::render
{
    namespace
    {
        constexpr std::string_view k_fullscreenVertexShader{"fullscreen.vert.spv"};
        constexpr std::string_view k_bloomPrefilterShader{"bloom_prefilter.frag.spv"};
        constexpr std::string_view k_bloomDownShader{"bloom_down.frag.spv"};
        constexpr std::string_view k_bloomUpShader{"bloom_up.frag.spv"};
        constexpr std::string_view k_tonemapShader{"tonemap.frag.spv"};
        constexpr uint32_t k_fullscreenTriangleVertices{3};
        constexpr uint32_t k_viewScene{0};
        constexpr uint32_t k_viewOcclusion{1};

        struct BloomConstants
        {
            float sourceTexel[4];
        };

        struct TonemapConstants
        {
            float exposure;
            uint32_t tonemapper;
            float bloomStrength;
            uint32_t view;
            float lift[4]{};
            float gamma[4]{};
            float gain[4]{};
            float shadowTint[4]{};
            float highlightTint[4]{};
            float vignette[4]{};
            float dirt[4]{};
        };

        static_assert(sizeof(BloomConstants) == 16);
        static_assert(sizeof(TonemapConstants) == 128);

        void store(float (&target)[4], const math::Vec3& xyz, float w)
        {
            target[0] = xyz.x;
            target[1] = xyz.y;
            target[2] = xyz.z;
            target[3] = w;
        }

        TonemapConstants tonemapConstants(const RendererState& state, float aspectRatio)
        {
            const RenderSettings& settings = state.settings;
            const TonemapSettings& tonemap = settings.tonemap;
            const ColorGradingSettings& grading = settings.colorGrading;
            const VignetteSettings& vignette = settings.vignette;
            const bool showOcclusion = settings.ambientOcclusion.enabled && settings.ambientOcclusion.showOnly;

            TonemapConstants constants{
                .exposure = tonemap.enabled ? tonemap.exposure : 1.0f,
                .tonemapper = static_cast<uint32_t>(tonemap.enabled ? tonemap.tonemapper : Tonemapper::Clamp),
                .bloomStrength = settings.bloom.enabled ? settings.bloom.strength : 0.0f,
                .view = showOcclusion ? k_viewOcclusion : k_viewScene};
            store(constants.lift, grading.lift, grading.enabled ? 1.0f : 0.0f);
            store(constants.gamma, grading.gamma, std::clamp(grading.contrast, 0.0f, 2.0f));
            store(constants.gain, grading.gain, std::max(grading.saturation, 0.0f));
            store(constants.shadowTint, grading.shadowTint, std::clamp(grading.splitToneStrength, 0.0f, 1.0f));
            store(constants.highlightTint, grading.highlightTint, grading.splitToneBalance);
            store(constants.vignette,
                math::Vec3{vignette.enabled ? std::clamp(vignette.intensity, 0.0f, 1.0f) : 0.0f, vignette.radius,
                    std::max(vignette.smoothness, 0.01f)},
                aspectRatio);
            const bool hasDirt = settings.lensDirt.enabled && settings.bloom.enabled;
            constants.dirt[0] = hasDirt ? std::clamp(settings.lensDirt.intensity, 0.0f, 2.0f) : 0.0f;

            return constants;
        }

        vela::graphics::AttachmentSlot colorSlot(std::string_view name, vela::graphics::TextureFormat format,
            float scale, float clearValue)
        {
            vela::graphics::AttachmentSlot slot{};
            slot.name = std::string(name);
            slot.format = format;
            slot.load = vela::graphics::LoadOp::Clear;
            slot.store = vela::graphics::StoreOp::Store;
            slot.clear = {clearValue, clearValue, clearValue, 1.0f};
            slot.scale = scale;

            return slot;
        }

        float bloomScale(uint32_t level)
        {
            return 1.0f / static_cast<float>(2u << level);
        }

        BloomConstants texelOf(const vela::graphics::PassRecorder& recorder, float relativeScale)
        {
            const vela::graphics::Extent2D extent = recorder.extent();
            const float width = static_cast<float>(extent.width) * relativeScale;
            const float height = static_cast<float>(extent.height) * relativeScale;

            return BloomConstants{.sourceTexel = {1.0f / width, 1.0f / height, 0.0f, 0.0f}};
        }

        Status addPass(vela::graphics::RenderGraph& graph, RendererState& state, std::string_view fragmentShader,
            vela::graphics::AttachmentSlot output, const std::vector<std::string>& inputs,
            FullscreenPass::Recorder recorder, const vela::graphics::Texture* materialTexture = nullptr)
        {
            std::vector<vela::graphics::PassInput> sampled;

            for (const std::string& input : inputs)
            {
                sampled.push_back({input, vela::graphics::InputUsage::Sampled});
            }

            return addFullscreenPass(graph, state, fragmentShader, std::move(output), std::move(sampled),
                std::move(recorder), materialTexture);
        }

        Status addBloomPasses(vela::graphics::RenderGraph& graph, RendererState& state)
        {
            const std::vector<std::string> sceneInputs{std::string(k_sceneColorAttachment),
                std::string(k_sceneAmbientAttachment), std::string(k_occlusionBlurAttachment)};

            if (auto added = addPass(graph, state, k_bloomPrefilterShader,
                colorSlot(k_bloomDownAttachments[0], k_sceneColorFormat, bloomScale(0), 0.0f), sceneInputs,
                [&state](vela::graphics::PassRecorder& recorder)
                {
                    recorder.setConstants(texelOf(recorder, 2.0f));
                    return state.settings.bloom.enabled;
                }); !added)
            {
                return added;
            }

            for (uint32_t level = 1; level < k_bloomLevels; ++level)
            {
                if (auto added = addPass(graph, state, k_bloomDownShader,
                    colorSlot(k_bloomDownAttachments[level], k_sceneColorFormat, bloomScale(level), 0.0f),
                    {std::string(k_bloomDownAttachments[level - 1])},
                    [&state](vela::graphics::PassRecorder& recorder)
                    {
                        recorder.setConstants(texelOf(recorder, 2.0f));
                        return state.settings.bloom.enabled;
                    }); !added)
                {
                    return added;
                }
            }

            if (auto added = addLensFlarePass(graph, state); !added)
            {
                return added;
            }

            for (uint32_t level = k_bloomLevels - 1; level-- > 0;)
            {
                const std::string_view lower = level + 1 == k_bloomLevels - 1 ? k_bloomDownAttachments[level + 1]
                    : k_bloomUpAttachments[level + 1];
                const bool carriesExposure = level == 0;
                std::vector<std::string> inputs{std::string(lower), std::string(k_bloomDownAttachments[level]),
                    std::string(k_exposureAttachment)};

                if (carriesExposure)
                {
                    inputs.emplace_back(k_lensFlareAttachment);
                }

                if (auto added = addPass(graph, state, carriesExposure ? k_bloomUpFlareShader : k_bloomUpShader,
                    colorSlot(k_bloomUpAttachments[level], k_sceneColorFormat, bloomScale(level), 0.0f),
                    inputs,
                    [&state, carriesExposure](vela::graphics::PassRecorder& recorder)
                    {
                        const AutoExposureSettings& exposure = state.settings.autoExposure;
                        BloomConstants constants = texelOf(recorder, 0.5f);
                        constants.sourceTexel[2] = carriesExposure && exposure.enabled ? 1.0f : 0.0f;
                        constants.sourceTexel[3] = exposure.compensationEv;
                        recorder.setConstants(constants);
                        return state.settings.bloom.enabled || carriesExposure;
                    }); !added)
                {
                    return added;
                }
            }

            return {};
        }

        Status addOutputPasses(vela::graphics::RenderGraph& graph, RendererState& state)
        {
            if (auto added = addPass(graph, state, k_tonemapShader,
                colorSlot(k_ldrAttachment, k_displayFormat, 1.0f, 0.0f),
                {std::string(k_sceneColorAttachment), std::string(k_sceneAmbientAttachment),
                    std::string(k_occlusionBlurAttachment), std::string(k_bloomUpAttachments[0])},
                [&state](vela::graphics::PassRecorder& recorder)
                {
                    const vela::graphics::Extent2D extent = recorder.extent();
                    const float aspectRatio = extent.height > 0
                        ? static_cast<float>(extent.width) / static_cast<float>(extent.height) : 1.0f;
                    recorder.setConstants(tonemapConstants(state, aspectRatio));
                    return true;
                }, &state.environment.lensDirt); !added)
            {
                return added;
            }

            if (auto added = addTemporalPass(graph, state); !added)
            {
                return added;
            }

            return addLensOutputPass(graph, state);
        }
    } //namespace

    Status addFullscreenPass(vela::graphics::RenderGraph& graph, RendererState& state, std::string_view fragmentShader,
        vela::graphics::AttachmentSlot output, std::vector<vela::graphics::PassInput> inputs,
        FullscreenPass::Recorder recorder, const vela::graphics::Texture* materialTexture)
    {
        vela::graphics::RenderState renderState{};
        renderState.depthTest = false;
        renderState.depthWrite = false;

        auto material = createBuiltinMaterial(state.context, k_fullscreenVertexShader, fragmentShader,
            vela::graphics::VertexLayout{}, renderState);

        if (!material)
        {
            return material.error();
        }

        if (materialTexture != nullptr)
        {
            if (auto bound = material.value().setTexture(0u, *materialTexture); !bound)
            {
                return toError(bound.error());
            }
        }

        const std::string name = output.name;
        auto pass = std::make_unique<FullscreenPass>(std::move(output), std::move(inputs),
            std::move(material).value(), std::move(recorder));

        return addScaledPass(graph, state, name, std::move(pass));
    }

    FullscreenPass::FullscreenPass(vela::graphics::AttachmentSlot output, std::vector<vela::graphics::PassInput> inputs,
        vela::graphics::Material&& material, Recorder recorder)
        : m_output(std::move(output)),
          m_inputs(std::move(inputs)),
          m_material(std::move(material)),
          m_recorder(std::move(recorder))
    {
    }

    vela::graphics::PassDescription FullscreenPass::describe() const
    {
        vela::graphics::PassDescription description{};
        description.colorOutputs.push_back(m_output);

        description.inputs = m_inputs;

        return description;
    }

    void FullscreenPass::record(vela::graphics::PassRecorder& recorder)
    {
        recorder.bind(m_material);

        for (uint32_t slot = 0; slot < m_inputs.size(); ++slot)
        {
            recorder.bindAttachment(slot, m_inputs[slot].name);
        }

        if (m_recorder(recorder))
        {
            recorder.drawInstanced(k_fullscreenTriangleVertices, 1);
        }
    }

    Status addPostPasses(vela::graphics::RenderGraph& graph, RendererState& state)
    {
        if (auto added = addExposurePasses(graph, state); !added)
        {
            return added;
        }

        if (auto added = addBloomPasses(graph, state); !added)
        {
            return added;
        }

        return addOutputPasses(graph, state);
    }
} //namespace lockstep::render
