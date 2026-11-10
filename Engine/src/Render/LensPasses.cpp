#include "LensPasses.hpp"

#include "FrameAttachments.hpp"
#include "PostPasses.hpp"
#include "RendererState.hpp"
#include "ViewModel.hpp"

#include <algorithm>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace lockstep::render
{
    namespace
    {
        constexpr std::string_view k_outputShader{"fxaa.frag.spv"};
        constexpr float k_ditherAmplitude{1.0f / 255.0f};
        constexpr float k_referenceHeight{1080.0f};
        constexpr float k_maxBlurTaps{16.0f};
        constexpr float k_maxDiscTaps{48.0f};

        enum class OutputFilter : uint32_t
        {
            None = 0,
            Fxaa,
            TemporalSharpen
        };

        struct OutputConstants
        {
            float fade[4];
            float inverseSize[2];
            OutputFilter filter;
            float dither;
            uint32_t frameIndex;
            float sharpness;
            float padding[2];
            float grain[4];
            float motionBlur[4];
            float depthOfField[4];
            float lens[4];
            float depthParams[4];
        };

        static_assert(sizeof(OutputConstants) == 128);

        OutputFilter outputFilter(AntiAliasingMode mode)
        {
            switch (mode)
            {
                case AntiAliasingMode::Fxaa:
                {
                    return OutputFilter::Fxaa;
                }
                case AntiAliasingMode::Taa:
                {
                    return OutputFilter::TemporalSharpen;
                }
                case AntiAliasingMode::Off:
                case AntiAliasingMode::Msaa2x:
                case AntiAliasingMode::Msaa4x:
                {
                    return OutputFilter::None;
                }
            }

            return OutputFilter::None;
        }

        vela::graphics::AttachmentSlot displaySlot()
        {
            vela::graphics::AttachmentSlot slot{};
            slot.name = std::string(k_displayAttachment);
            slot.format = k_displayFormat;
            slot.load = vela::graphics::LoadOp::Clear;
            slot.store = vela::graphics::StoreOp::Store;
            slot.clear = {0.0f, 0.0f, 0.0f, 1.0f};

            return slot;
        }

        OutputConstants outputConstants(const RendererState& state, const vela::graphics::Extent2D& extent)
        {
            const RenderSettings& settings = state.settings;
            const AntiAliasingSettings& antiAliasing = settings.antiAliasing;
            const FilmGrainSettings& grain = settings.filmGrain;
            const MotionBlurSettings& blur = settings.motionBlur;
            const DepthOfFieldSettings& focus = settings.depthOfField;
            const ChromaticAberrationSettings& aberration = settings.chromaticAberration;
            const math::Vec4& fade = state.screenFade;
            const float pixelScale = static_cast<float>(extent.height) / k_referenceHeight;
            const float dofStrength = !focus.enabled ? 0.0f : focus.preview ? 1.0f : std::clamp(focus.strength, 0.0f, 1.0f);

            return OutputConstants{
                .fade = {fade.x, fade.y, fade.z, fade.w},
                .inverseSize = {1.0f / static_cast<float>(extent.width), 1.0f / static_cast<float>(extent.height)},
                .filter = outputFilter(antiAliasing.mode),
                .dither = settings.tonemap.dither ? k_ditherAmplitude : 0.0f,
                .frameIndex = state.frameIndex,
                .sharpness = antiAliasing.sharpness,
                .padding = {0.0f, 0.0f},
                .grain = {grain.enabled ? std::max(grain.intensity, 0.0f) : 0.0f, std::clamp(grain.response, 0.0f, 1.0f),
                    std::clamp(grain.size, 1.0f, 3.0f), 0.0f},
                .motionBlur = {blur.enabled ? std::clamp(blur.intensity, 0.0f, 1.0f) : 0.0f,
                    std::max(blur.maxPixels, 1.0f) * pixelScale,
                    std::clamp(static_cast<float>(blur.samples), 4.0f, k_maxBlurTaps),
                    std::clamp(blur.viewModelScale, 0.0f, 1.0f)},
                .depthOfField = {dofStrength, std::max(focus.focusDistance, 0.0f), std::max(focus.focusTolerance, 0.0f),
                    std::max(focus.blurScale, 0.0f)},
                .lens = {std::max(focus.maxRadius, 0.0f) * pixelScale,
                    std::clamp(static_cast<float>(focus.samples), 8.0f, k_maxDiscTaps),
                    aberration.enabled ? std::max(aberration.intensity, 0.0f) * pixelScale : 0.0f, 0.0f},
                .depthParams = {state.unjitteredProjection[2][2], state.unjitteredProjection[3][2],
                    k_viewModelDepthScale, 0.0f}};
        }
    } //namespace

    Status addLensOutputPass(vela::graphics::RenderGraph& graph, RendererState& state)
    {
        std::vector<vela::graphics::PassInput> inputs;

        for (const std::string_view input : {k_ldrAttachment, k_taaHistoryAttachment, k_sceneDepthAttachment,
            k_sceneVelocityAttachment})
        {
            inputs.push_back({std::string(input), vela::graphics::InputUsage::Sampled});
        }

        return addFullscreenPass(graph, state, k_outputShader, displaySlot(), std::move(inputs),
            [&state](vela::graphics::PassRecorder& recorder)
            {
                recorder.setConstants(outputConstants(state, recorder.extent()));
                return true;
            });
    }
} //namespace lockstep::render
