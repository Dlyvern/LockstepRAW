#include "Lockstep/Render/GraphicsQuality.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>

namespace lockstep::render
{
    namespace
    {
        constexpr std::array k_presets{GraphicsPreset::Low, GraphicsPreset::Medium, GraphicsPreset::High,
            GraphicsPreset::Ultra, GraphicsPreset::Custom};
        constexpr float k_minEffectSteps{0.25f};
        constexpr float k_maxEffectSteps{2.0f};
        constexpr float k_minResolutionMultiplier{0.25f};
        constexpr float k_maxResolutionMultiplier{2.0f};
        constexpr uint32_t k_minScopeResolution{256};
        constexpr uint32_t k_maxScopeResolution{1024};
        constexpr uint32_t k_minParticleBudget{1000};
        constexpr uint32_t k_maxParticleBudget{100000};

        uint32_t scaledSteps(uint32_t steps, float scale, uint32_t lowest, uint32_t highest)
        {
            const float scaled = std::round(static_cast<float>(steps) * scale);

            return std::clamp(static_cast<uint32_t>(std::max(scaled, 0.0f)), lowest, highest);
        }

        GraphicsQuality lowQuality()
        {
            return GraphicsQuality{
                .preset = GraphicsPreset::Low,
                .renderScale = 0.75f,
                .textureMipDrop = 2,
                .antiAliasing = AntiAliasingMode::Fxaa,
                .shadowAtlasSize = 2048,
                .shadowCascades = 2,
                .shadowedLocalLights = 2,
                .volumetricFog = true,
                .volumetricFogResolution = 0.5f,
                .screenSpaceReflections = false,
                .ambientOcclusion = true,
                .contactShadows = false,
                .effectSteps = 0.5f,
                .parallax = false,
                .surfaceDetail = false,
                .motionBlur = false,
                .depthOfField = false,
                .lensEffects = false,
                .bloom = true,
                .planarReflections = true,
                .reflectionResolution = 0.5f,
                .scopeResolution = 256,
                .particleBudget = 5000};
        }

        GraphicsQuality mediumQuality()
        {
            return GraphicsQuality{
                .preset = GraphicsPreset::Medium,
                .renderScale = 1.0f,
                .textureMipDrop = 1,
                .antiAliasing = AntiAliasingMode::Taa,
                .shadowAtlasSize = 4096,
                .shadowCascades = 3,
                .shadowedLocalLights = 3,
                .volumetricFog = true,
                .volumetricFogResolution = 0.75f,
                .screenSpaceReflections = true,
                .ambientOcclusion = true,
                .contactShadows = true,
                .effectSteps = 0.75f,
                .parallax = true,
                .surfaceDetail = true,
                .motionBlur = true,
                .depthOfField = true,
                .lensEffects = true,
                .bloom = true,
                .planarReflections = true,
                .reflectionResolution = 0.75f,
                .scopeResolution = 512,
                .particleBudget = 10000};
        }

        GraphicsQuality ultraQuality()
        {
            return GraphicsQuality{
                .preset = GraphicsPreset::Ultra,
                .renderScale = 1.0f,
                .textureMipDrop = 0,
                .antiAliasing = AntiAliasingMode::Taa,
                .shadowAtlasSize = 8192,
                .shadowCascades = 4,
                .shadowedLocalLights = 8,
                .volumetricFog = true,
                .volumetricFogResolution = 1.5f,
                .screenSpaceReflections = true,
                .ambientOcclusion = true,
                .contactShadows = true,
                .effectSteps = 1.5f,
                .parallax = true,
                .surfaceDetail = true,
                .motionBlur = true,
                .depthOfField = true,
                .lensEffects = true,
                .bloom = true,
                .planarReflections = true,
                .reflectionResolution = 1.5f,
                .scopeResolution = 1024,
                .particleBudget = 20000};
        }

        void limitIndirectLighting(RenderSettings& settings, const GraphicsQuality& quality)
        {
            AmbientOcclusionSettings& occlusion = settings.ambientOcclusion;
            occlusion.enabled = occlusion.enabled && quality.ambientOcclusion;
            occlusion.slices = scaledSteps(occlusion.slices, std::min(quality.effectSteps, 1.0f), 1, 4);
            occlusion.steps = scaledSteps(occlusion.steps, quality.effectSteps, 2, 12);

            ContactShadowSettings& contact = settings.contactShadows;
            contact.enabled = contact.enabled && quality.contactShadows;
            contact.steps = scaledSteps(contact.steps, quality.effectSteps, 4, 24);

            ScreenSpaceReflectionSettings& reflections = settings.screenSpaceReflections;
            reflections.enabled = reflections.enabled && quality.screenSpaceReflections;
            reflections.steps = scaledSteps(reflections.steps, quality.effectSteps, 8, 64);

            VolumetricFogSettings& fog = settings.volumetricFog;
            fog.enabled = fog.enabled && quality.volumetricFog;
            fog.steps = scaledSteps(fog.steps, quality.effectSteps, 8, 64);
            fog.resolutionScale = std::clamp(fog.resolutionScale * quality.volumetricFogResolution, 0.25f, 1.0f);
        }

        void limitSurfacesAndLens(RenderSettings& settings, const GraphicsQuality& quality)
        {
            SurfaceDetailSettings& detail = settings.surfaceDetail;
            detail.parallax = detail.parallax && quality.parallax;
            detail.parallaxShadows = detail.parallaxShadows && quality.parallax;
            detail.parallaxMinSteps = scaledSteps(detail.parallaxMinSteps, quality.effectSteps, 4, 64);
            detail.parallaxMaxSteps = scaledSteps(detail.parallaxMaxSteps, quality.effectSteps,
                detail.parallaxMinSteps, 64);
            detail.detail = detail.detail && quality.surfaceDetail;

            settings.motionBlur.enabled = settings.motionBlur.enabled && quality.motionBlur;
            settings.motionBlur.samples = scaledSteps(settings.motionBlur.samples, quality.effectSteps, 4, 16);
            settings.depthOfField.enabled = settings.depthOfField.enabled && quality.depthOfField;
            settings.depthOfField.samples = scaledSteps(settings.depthOfField.samples, quality.effectSteps, 8, 48);
            settings.chromaticAberration.enabled = settings.chromaticAberration.enabled && quality.lensEffects;
            settings.lensDirt.enabled = settings.lensDirt.enabled && quality.lensEffects;
            settings.bloom.enabled = settings.bloom.enabled && quality.bloom;
        }
    } //namespace

    GraphicsQuality graphicsQualityPreset(GraphicsPreset preset)
    {
        switch (preset)
        {
            case GraphicsPreset::Low:
            {
                return lowQuality();
            }
            case GraphicsPreset::Medium:
            {
                return mediumQuality();
            }
            case GraphicsPreset::Ultra:
            {
                return ultraQuality();
            }
            case GraphicsPreset::High:
            case GraphicsPreset::Custom:
            {
                break;
            }
        }

        return GraphicsQuality{};
    }

    const char* toString(GraphicsPreset preset)
    {
        switch (preset)
        {
            case GraphicsPreset::Low:
            {
                return "low";
            }
            case GraphicsPreset::Medium:
            {
                return "medium";
            }
            case GraphicsPreset::High:
            {
                return "high";
            }
            case GraphicsPreset::Ultra:
            {
                return "ultra";
            }
            case GraphicsPreset::Custom:
            {
                return "custom";
            }
        }

        return "high";
    }

    std::optional<GraphicsPreset> parseGraphicsPreset(std::string_view name)
    {
        for (const GraphicsPreset preset : k_presets)
        {
            if (name == toString(preset))
            {
                return preset;
            }
        }

        return std::nullopt;
    }

    GraphicsQuality clampedQuality(const GraphicsQuality& quality)
    {
        GraphicsQuality clamped = quality;
        clamped.renderScale = std::clamp(quality.renderScale, k_minRenderScale, k_maxRenderScale);
        clamped.textureMipDrop = std::min(quality.textureMipDrop, k_maxTextureMipDrop);
        clamped.shadowAtlasSize = std::bit_floor(std::clamp(quality.shadowAtlasSize, k_minShadowAtlasSize,
            k_maxShadowAtlasSize));
        clamped.shadowCascades = std::clamp(quality.shadowCascades, 1u, k_maxShadowCascades);
        clamped.shadowedLocalLights = std::min(quality.shadowedLocalLights, k_maxShadowedLocalLights);
        clamped.volumetricFogResolution = std::clamp(quality.volumetricFogResolution, k_minResolutionMultiplier,
            k_maxResolutionMultiplier);
        clamped.effectSteps = std::clamp(quality.effectSteps, k_minEffectSteps, k_maxEffectSteps);
        clamped.reflectionResolution = std::clamp(quality.reflectionResolution, k_minResolutionMultiplier,
            k_maxResolutionMultiplier);
        clamped.scopeResolution = std::clamp(quality.scopeResolution, k_minScopeResolution, k_maxScopeResolution);
        clamped.particleBudget = std::clamp(quality.particleBudget, k_minParticleBudget, k_maxParticleBudget);
        clamped.frameRateLimit = quality.frameRateLimit == 0 ? 0
            : std::clamp(quality.frameRateLimit, k_minFrameRateLimit, k_maxFrameRateLimit);

        return clamped;
    }

    RenderSettings limitedByQuality(const RenderSettings& requested, const GraphicsQuality& quality)
    {
        RenderSettings settings = requested;
        settings.antiAliasing.mode = quality.antiAliasing;
        settings.shadows.cascadeCount = quality.shadowCascades;
        settings.shadows.maxLocalLights = quality.shadowedLocalLights;
        settings.shadows.maskedCasterCascades = std::min(settings.shadows.maskedCasterCascades, quality.shadowCascades);
        settings.reflections.enabled = settings.reflections.enabled && quality.planarReflections;
        settings.reflections.resolutionScale = std::clamp(
            settings.reflections.resolutionScale * quality.reflectionResolution, 0.25f, 1.0f);
        settings.scope.resolution = std::min(settings.scope.resolution, quality.scopeResolution);
        limitIndirectLighting(settings, quality);
        limitSurfacesAndLens(settings, quality);

        return settings;
    }
} //namespace lockstep::render
