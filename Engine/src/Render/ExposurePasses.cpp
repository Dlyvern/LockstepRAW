#include "ExposurePasses.hpp"

#include "FrameAttachments.hpp"
#include "PostPasses.hpp"
#include "RendererState.hpp"

#include <Vela/Graphics/RenderGraph.hpp>

#include <algorithm>
#include <cmath>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace lockstep::render
{
    namespace
    {
        constexpr std::string_view k_luminanceShader{"luminance.frag.spv"};
        constexpr std::string_view k_luminanceReduceShader{"luminance_reduce.frag.spv"};
        constexpr std::string_view k_exposureShader{"exposure.frag.spv"};
        constexpr float k_minTargetLuminance{1e-3f};

        struct GridConstants
        {
            float params[4];
        };

        struct ExposureConstants
        {
            float target[4];
            float adaptation[4];
        };

        static_assert(sizeof(GridConstants) == 16);
        static_assert(sizeof(ExposureConstants) == 32);

        vela::graphics::AttachmentSlot fixedSlot(std::string_view name, uint32_t size)
        {
            vela::graphics::AttachmentSlot slot{};
            slot.name = std::string(name);
            slot.format = k_sceneColorFormat;
            slot.load = vela::graphics::LoadOp::Clear;
            slot.store = vela::graphics::StoreOp::Store;
            slot.clear = {0.0f, 0.0f, 0.0f, 0.0f};
            slot.size = {size, size};

            return slot;
        }

        std::vector<vela::graphics::PassInput> sampled(std::vector<std::string_view> names)
        {
            std::vector<vela::graphics::PassInput> inputs;

            for (const std::string_view name : names)
            {
                inputs.push_back({std::string(name), vela::graphics::InputUsage::Sampled});
            }

            return inputs;
        }
    } //namespace

    Status addExposurePasses(vela::graphics::RenderGraph& graph, RendererState& state)
    {
        if (auto added = addFullscreenPass(graph, state, k_luminanceShader,
            fixedSlot(k_luminanceAttachment, k_luminanceGridSize),
            sampled({k_sceneColorAttachment, k_sceneAmbientAttachment, k_occlusionBlurAttachment}),
            [&state](vela::graphics::PassRecorder& recorder)
            {
                const float grid = static_cast<float>(k_luminanceGridSize);
                recorder.setConstants(GridConstants{
                    .params = {grid, grid, std::clamp(state.settings.autoExposure.centreWeight, 0.0f, 1.0f), 0.0f}});
                return true;
            }); !added)
        {
            return added;
        }

        if (auto added = addFullscreenPass(graph, state, k_luminanceReduceShader,
            fixedSlot(k_luminanceReducedAttachment, k_luminanceReducedSize), sampled({k_luminanceAttachment}),
            [](vela::graphics::PassRecorder& recorder)
            {
                const float block = static_cast<float>(k_luminanceGridSize / k_luminanceReducedSize);
                recorder.setConstants(GridConstants{.params = {block, 0.0f, 0.0f, 0.0f}});
                return true;
            }); !added)
        {
            return added;
        }

        vela::graphics::AttachmentSlot exposure = fixedSlot(k_exposureAttachment, 1);
        exposure.persistent = true;
        std::vector<vela::graphics::PassInput> inputs = sampled({k_luminanceReducedAttachment});
        inputs.push_back({std::string(k_exposureAttachment), vela::graphics::InputUsage::History});

        return addFullscreenPass(graph, state, k_exposureShader, std::move(exposure), std::move(inputs),
            [&state](vela::graphics::PassRecorder& recorder)
            {
                const AutoExposureSettings& settings = state.settings.autoExposure;
                const float target = std::log2(std::max(settings.targetLuminance, k_minTargetLuminance));
                recorder.setConstants(ExposureConstants{
                    .target = {target, settings.minEv, std::max(settings.maxEv, settings.minEv), 0.0f},
                    .adaptation = {std::max(settings.brightenSpeed, 0.0f), std::max(settings.darkenSpeed, 0.0f),
                        state.frameSeconds, 0.0f}});
                return true;
            });
    }
} //namespace lockstep::render
