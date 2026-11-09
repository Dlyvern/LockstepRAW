#include "PostPasses.hpp"

#include "FrameAttachments.hpp"
#include "FramePasses.hpp"
#include "RendererState.hpp"
#include "VelaConversions.hpp"

#include <Vela/Graphics/RenderGraph.hpp>
#include <Vela/Graphics/VertexLayout.hpp>

#include <cstdint>
#include <string>
#include <string_view>
#include <utility>

namespace lockstep::render
{
    namespace
    {
        constexpr std::string_view k_fullscreenVertexShader{"fullscreen.vert.spv"};
        constexpr std::string_view k_ssaoShader{"ssao.frag.spv"};
        constexpr std::string_view k_ssaoBlurShader{"ssao_blur.frag.spv"};
        constexpr std::string_view k_bloomPrefilterShader{"bloom_prefilter.frag.spv"};
        constexpr std::string_view k_bloomDownShader{"bloom_down.frag.spv"};
        constexpr std::string_view k_bloomUpShader{"bloom_up.frag.spv"};
        constexpr std::string_view k_tonemapShader{"tonemap.frag.spv"};
        constexpr std::string_view k_fxaaShader{"fxaa.frag.spv"};
        constexpr uint32_t k_fullscreenTriangleVertices{3};
        constexpr float k_occlusionScale{0.5f};
        constexpr float k_ditherAmplitude{1.0f / 255.0f};
        constexpr uint32_t k_viewScene{0};
        constexpr uint32_t k_viewOcclusion{1};

        struct ProjectionConstants
        {
            float projection[4];
            float params[4];
        };

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
        };

        struct OutputConstants
        {
            float inverseSize[2];
            float enabled;
            float dither;
            uint32_t frameIndex;
        };

        static_assert(sizeof(ProjectionConstants) == 32);
        static_assert(sizeof(BloomConstants) == 16);
        static_assert(sizeof(TonemapConstants) == 16);
        static_assert(sizeof(OutputConstants) == 20);

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

        ProjectionConstants projectionConstants(const RendererState& state)
        {
            const math::Mat4& projection = state.projection;
            const AmbientOcclusionSettings& settings = state.settings.ambientOcclusion;

            return ProjectionConstants{
                .projection = {projection[0][0], projection[1][1], projection[2][2], projection[3][2]},
                .params = {settings.radius, settings.intensity, settings.power, 0.0f}};
        }

        BloomConstants texelOf(const vela::graphics::PassRecorder& recorder, float relativeScale)
        {
            const vela::graphics::Extent2D extent = recorder.extent();
            const float width = static_cast<float>(extent.width) * relativeScale;
            const float height = static_cast<float>(extent.height) * relativeScale;

            return BloomConstants{.sourceTexel = {1.0f / width, 1.0f / height, 0.0f, 0.0f}};
        }

        Status addPass(vela::graphics::RenderGraph& graph, RendererState& state, std::string_view fragmentShader,
            vela::graphics::AttachmentSlot output, std::vector<std::string> inputs, FullscreenPass::Recorder recorder)
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

            const std::string name = output.name;
            auto pass = std::make_unique<FullscreenPass>(std::move(output), std::move(inputs),
                std::move(material).value(), std::move(recorder));

            if (auto added = graph.addPass(name, std::move(pass)); !added)
            {
                return toError(added.error());
            }

            return {};
        }

        Status addOcclusionPasses(vela::graphics::RenderGraph& graph, RendererState& state)
        {
            const std::string depth(k_sceneDepthAttachment);

            if (auto added = addPass(graph, state, k_ssaoShader,
                colorSlot(k_occlusionAttachment, k_occlusionFormat, k_occlusionScale, 1.0f), {depth},
                [&state](vela::graphics::PassRecorder& recorder)
                {
                    recorder.setConstants(projectionConstants(state));
                    return state.settings.ambientOcclusion.enabled;
                }); !added)
            {
                return added;
            }

            return addPass(graph, state, k_ssaoBlurShader,
                colorSlot(k_occlusionBlurAttachment, k_occlusionFormat, k_occlusionScale, 1.0f),
                {std::string(k_occlusionAttachment), depth},
                [&state](vela::graphics::PassRecorder& recorder)
                {
                    recorder.setConstants(projectionConstants(state));
                    return state.settings.ambientOcclusion.enabled;
                });
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

            for (uint32_t level = k_bloomLevels - 1; level-- > 0;)
            {
                const std::string_view lower = level + 1 == k_bloomLevels - 1 ? k_bloomDownAttachments[level + 1]
                    : k_bloomUpAttachments[level + 1];

                if (auto added = addPass(graph, state, k_bloomUpShader,
                    colorSlot(k_bloomUpAttachments[level], k_sceneColorFormat, bloomScale(level), 0.0f),
                    {std::string(lower), std::string(k_bloomDownAttachments[level])},
                    [&state](vela::graphics::PassRecorder& recorder)
                    {
                        recorder.setConstants(texelOf(recorder, 0.5f));
                        return state.settings.bloom.enabled;
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
                    const RenderSettings& settings = state.settings;
                    const TonemapSettings& tonemap = settings.tonemap;
                    const bool showOcclusion = settings.ambientOcclusion.enabled && settings.ambientOcclusion.showOnly;

                    recorder.setConstants(TonemapConstants{
                        .exposure = tonemap.enabled ? tonemap.exposure : 1.0f,
                        .tonemapper = static_cast<uint32_t>(tonemap.enabled ? tonemap.tonemapper : Tonemapper::Clamp),
                        .bloomStrength = settings.bloom.enabled ? settings.bloom.strength : 0.0f,
                        .view = showOcclusion ? k_viewOcclusion : k_viewScene});
                    return true;
                }); !added)
            {
                return added;
            }

            return addPass(graph, state, k_fxaaShader, colorSlot(k_displayAttachment, k_displayFormat, 1.0f, 0.0f),
                {std::string(k_ldrAttachment)},
                [&state](vela::graphics::PassRecorder& recorder)
                {
                    const vela::graphics::Extent2D extent = recorder.extent();

                    recorder.setConstants(OutputConstants{
                        .inverseSize = {1.0f / static_cast<float>(extent.width), 1.0f / static_cast<float>(extent.height)},
                        .enabled = state.settings.antiAliasing.fxaa ? 1.0f : 0.0f,
                        .dither = state.settings.tonemap.dither ? k_ditherAmplitude : 0.0f,
                        .frameIndex = state.frameIndex});
                    return true;
                });
        }
    } //namespace

    FullscreenPass::FullscreenPass(vela::graphics::AttachmentSlot output, std::vector<std::string> inputs,
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

        for (const std::string& input : m_inputs)
        {
            description.inputs.push_back({input, vela::graphics::InputUsage::Sampled});
        }

        return description;
    }

    void FullscreenPass::record(vela::graphics::PassRecorder& recorder)
    {
        recorder.bind(m_material);

        for (uint32_t slot = 0; slot < m_inputs.size(); ++slot)
        {
            recorder.bindAttachment(slot, m_inputs[slot]);
        }

        if (m_recorder(recorder))
        {
            recorder.drawInstanced(k_fullscreenTriangleVertices, 1);
        }
    }

    Status addPostPasses(vela::graphics::RenderGraph& graph, RendererState& state)
    {
        if (auto added = addOcclusionPasses(graph, state); !added)
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
