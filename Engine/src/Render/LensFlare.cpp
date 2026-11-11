#include "LensFlare.hpp"

#include "FrameAttachments.hpp"
#include "PostPasses.hpp"
#include "RendererState.hpp"
#include "ViewModel.hpp"

#include <algorithm>
#include <cmath>
#include <string>
#include <utility>
#include <vector>

namespace lockstep::render
{
    namespace
    {
        constexpr std::string_view k_flareShader{"lens_flare.frag.spv"};
        constexpr uint32_t k_flareSourceLevel{1};
        constexpr float k_flareScale{0.25f};
        constexpr float k_ghostReflectance{0.006f};
        constexpr float k_haloReflectance{0.012f};
        constexpr float k_sunGlareShare{0.12f};
        constexpr float k_maxIntensity{4.0f};
        constexpr float k_sunsetElevation{0.35f};
        constexpr float k_starburstRotation{0.3f};
        constexpr float k_bloomLevelCount{static_cast<float>(k_bloomLevels)};
        constexpr float k_minBloomStrength{1e-3f};

        struct FlareConstants
        {
            float ghosts[4];
            float lens[4];
            float sun[4];
            float sunColor[4];
            float exposure[4];
        };

        static_assert(sizeof(FlareConstants) == 80);

        float limited(float value)
        {
            return std::clamp(value, 0.0f, k_maxIntensity);
        }

        bool sunOnScreen(const RendererState& state, float (&uv)[2])
        {
            const GpuFrameLighting& lighting = state.prepared.lighting;

            if (lighting.sunDirection[3] <= 0.0f)
            {
                return false;
            }

            const math::Vec4 towardSun{-lighting.skySunDirection[0], -lighting.skySunDirection[1],
                -lighting.skySunDirection[2], 0.0f};
            const math::Vec4 clip = lighting.viewProjection * towardSun;

            if (clip.w <= 1e-4f)
            {
                return false;
            }

            uv[0] = clip.x / clip.w * 0.5f + 0.5f;
            uv[1] = clip.y / clip.w * 0.5f + 0.5f;

            return uv[0] > -0.1f && uv[0] < 1.1f && uv[1] > -0.1f && uv[1] < 1.1f;
        }

        float sunsetFactor(const RendererState& state, const LensFlareSettings& flare)
        {
            const float elevation = -state.prepared.lighting.skySunDirection[1];
            const float low = 1.0f - std::clamp(elevation / k_sunsetElevation, 0.0f, 1.0f);
            const float aboveHorizon = std::clamp(elevation * 20.0f + 0.5f, 0.0f, 1.0f);

            return (1.0f + std::max(flare.sunsetBoost, 0.0f) * low * low) * aboveHorizon;
        }

        FlareConstants flareConstants(const RendererState& state, float aspectRatio)
        {
            const RenderSettings& settings = state.settings;
            const LensFlareSettings& flare = settings.lensFlare;
            const AutoExposureSettings& exposure = settings.autoExposure;
            const float toBloom = k_bloomLevelCount / std::max(settings.bloom.strength, k_minBloomStrength);

            FlareConstants constants{};
            constants.ghosts[0] = flare.enabled ? limited(flare.intensity) * k_ghostReflectance : 0.0f;
            constants.ghosts[1] = std::max(flare.threshold, 0.0f);
            constants.ghosts[2] = flare.enabled ? limited(flare.haloIntensity) * k_haloReflectance : 0.0f;
            constants.ghosts[3] = std::clamp(flare.haloRadius, 0.05f, 0.5f);
            constants.lens[0] = std::clamp(flare.chromaticShift, 0.0f, 0.05f);
            constants.lens[1] = aspectRatio;
            constants.lens[2] = k_viewModelDepthScale;
            constants.lens[3] = toBloom;

            float sunUv[2]{};

            if (flare.sunGlare && sunOnScreen(state, sunUv))
            {
                const GpuFrameLighting& lighting = state.prepared.lighting;
                constants.sun[0] = sunUv[0];
                constants.sun[1] = sunUv[1];
                constants.sun[2] = limited(flare.sunGlareIntensity) * k_sunGlareShare * sunsetFactor(state, flare);
                constants.sun[3] = std::clamp(flare.starburst, 0.0f, 1.0f);
                constants.sunColor[0] = lighting.skySunRadiance[0];
                constants.sunColor[1] = lighting.skySunRadiance[1];
                constants.sunColor[2] = lighting.skySunRadiance[2];
                constants.sunColor[3] = k_starburstRotation;
            }

            constants.exposure[0] = exposure.enabled ? 1.0f : 0.0f;
            constants.exposure[1] = exposure.compensationEv;
            constants.exposure[2] = settings.tonemap.enabled ? settings.tonemap.exposure : 1.0f;

            return constants;
        }

        bool isDrawn(const RenderSettings& settings)
        {
            return settings.bloom.enabled && (settings.lensFlare.enabled || settings.lensFlare.sunGlare);
        }
    } //namespace

    Status addLensFlarePass(vela::graphics::RenderGraph& graph, RendererState& state)
    {
        vela::graphics::AttachmentSlot output{};
        output.name = std::string(k_lensFlareAttachment);
        output.format = k_sceneColorFormat;
        output.load = vela::graphics::LoadOp::Clear;
        output.store = vela::graphics::StoreOp::Store;
        output.clear = {0.0f, 0.0f, 0.0f, 1.0f};
        output.scale = k_flareScale;

        std::vector<vela::graphics::PassInput> inputs;

        for (const std::string_view input : {k_bloomDownAttachments[k_flareSourceLevel], k_sceneDepthAttachment,
            k_exposureAttachment})
        {
            inputs.push_back({std::string(input), vela::graphics::InputUsage::Sampled});
        }

        return addFullscreenPass(graph, state, k_flareShader, std::move(output), std::move(inputs),
            [&state](vela::graphics::PassRecorder& recorder)
            {
                if (!isDrawn(state.settings))
                {
                    return false;
                }

                const vela::graphics::Extent2D extent = recorder.extent();
                const float aspectRatio = extent.height > 0
                    ? static_cast<float>(extent.width) / static_cast<float>(extent.height) : 1.0f;
                recorder.setConstants(flareConstants(state, aspectRatio));
                return true;
            });
    }
} //namespace lockstep::render
