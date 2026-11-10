#include "Presentation/LevelLook.hpp"

#include <Lockstep/Core/Math.hpp>
#include <Lockstep/Render/RenderSettings.hpp>
#include <Lockstep/Render/Renderer.hpp>

#include <algorithm>
#include <cctype>
#include <string>

namespace game
{
    namespace
    {
        namespace assets = lockstep::assets;
        namespace render = lockstep::render;
        namespace math = lockstep::math;

        constexpr std::string_view k_lookPrefix{"level_look_"};

        std::string lowered(std::string_view text)
        {
            std::string result(text);
            std::transform(result.begin(), result.end(), result.begin(),
                [](unsigned char character) { return static_cast<char>(std::tolower(character)); });
            return result;
        }

        void applyLens(render::RenderSettings& settings)
        {
            settings.surfaceDetail = render::SurfaceDetailSettings{};
            settings.motionBlur = {.enabled = true, .intensity = 0.35f, .maxPixels = 20.0f, .samples = 8,
                .viewModelScale = 0.15f};
            settings.chromaticAberration = {.enabled = true, .intensity = 1.2f};
            settings.lensDirt = {.enabled = true, .intensity = 0.3f};
            settings.depthOfField = {.enabled = true, .strength = settings.depthOfField.strength, .focusDistance = 0.0f,
                .focusTolerance = 0.2f, .blurScale = 1.5f, .maxRadius = 7.0f, .samples = 16, .preview = false};
        }

        void applyGrade(render::RenderSettings& settings)
        {
            settings.interiorAmbient = {.enabled = true, .diffuseIntensity = 0.1f, .specularIntensity = 0.15f,
                .fillColor = {0.55f, 0.62f, 0.72f}, .fillIntensity = 0.035f, .blendDistance = 0.6f,
                .normalOffset = 0.3f, .debugView = false};
            settings.autoExposure = {.enabled = true, .targetLuminance = 0.09f, .minEv = -2.5f, .maxEv = 1.0f,
                .compensationEv = 0.0f, .brightenSpeed = 0.7f, .darkenSpeed = 2.5f, .centreWeight = 0.6f};
            settings.colorGrading = {.enabled = true, .lift = {-0.012f, -0.006f, 0.008f}, .gamma = {1.0f, 1.0f, 1.0f},
                .gain = {1.02f, 1.0f, 0.96f}, .contrast = 1.15f, .saturation = 0.8f,
                .shadowTint = {0.45f, 0.6f, 0.75f}, .highlightTint = {0.85f, 0.7f, 0.5f}, .splitToneStrength = 0.25f,
                .splitToneBalance = 0.45f};
            settings.vignette = {.enabled = true, .intensity = 0.3f, .radius = 0.5f, .smoothness = 0.6f};
            settings.filmGrain = {.enabled = true, .intensity = 0.035f, .response = 0.7f, .size = 1.4f};
            settings.fog = render::FogSettings{};
            settings.volumetricFog = {.enabled = true, .density = 0.008f, .albedo = {0.85f, 0.87f, 0.9f},
                .anisotropy = 0.45f, .heightFalloff = 0.0f, .baseHeight = 0.0f, .sunIntensity = 1.0f,
                .localLightIntensity = 1.0f, .ambientIntensity = 0.01f, .interiorDensityScale = 2.5f,
                .maxDistance = 40.0f, .steps = 24, .resolutionScale = 0.5f, .temporal = true, .historyWeight = 0.9f};
            settings.bloom = render::BloomSettings{};
            applyLens(settings);
            settings.ambientOcclusion = {.enabled = true, .radius = 0.7f, .intensity = 1.0f, .power = 1.3f,
                .slices = 2, .steps = 6, .multiBounce = true, .multiBounceAlbedo = 0.5f, .specularOcclusion = true,
                .showOnly = false};
            settings.contactShadows = {.enabled = true, .length = 0.35f, .thickness = 0.12f, .intensity = 0.85f,
                .steps = 12};
            settings.screenSpaceReflections = {.enabled = true, .intensity = 1.0f, .maxRoughness = 0.55f,
                .maxDistance = 25.0f, .steps = 32, .thickness = 0.3f, .edgeFade = 0.08f, .temporal = true,
                .historyWeight = 0.88f, .debugView = false};
        }

        void applyDay(render::RenderSettings& settings)
        {
            settings.lighting.directional = true;
            settings.sky = render::SkySettings{};
            settings.sky.sunAzimuth = math::radians(145.0f);
            settings.sky.sunElevation = math::radians(38.0f);
            settings.ibl = {.enabled = true, .diffuseIntensity = 1.0f, .specularIntensity = 1.0f};
            settings.volumetricFog.density = 0.005f;
        }

        void applyEvening(render::RenderSettings& settings)
        {
            settings.lighting.directional = true;
            settings.sky = render::SkySettings{};
            settings.sky.sunAzimuth = math::radians(250.0f);
            settings.sky.sunElevation = math::radians(5.0f);
            settings.sky.sunColor = {1.0f, 0.62f, 0.38f};
            settings.sky.sunIntensity = 1.3f;
            settings.sky.atmosphericSunTint = false;
            settings.sky.turbidity = 3.0f;
            settings.sky.skyIntensity = 0.25f;
            settings.sky.groundAlbedo = {0.08f, 0.08f, 0.09f};
            settings.ibl = {.enabled = true, .diffuseIntensity = 0.45f, .specularIntensity = 0.6f};
            settings.autoExposure.targetLuminance = 0.07f;
            settings.autoExposure.minEv = -1.5f;
            settings.autoExposure.maxEv = 1.5f;
            settings.colorGrading.saturation = 0.7f;
            settings.colorGrading.shadowTint = {0.3f, 0.45f, 0.85f};
            settings.colorGrading.highlightTint = {0.9f, 0.72f, 0.5f};
            settings.colorGrading.splitToneStrength = 0.45f;
            settings.colorGrading.splitToneBalance = 0.6f;
            settings.volumetricFog.density = 0.012f;
            settings.volumetricFog.albedo = {0.9f, 0.82f, 0.75f};
            settings.lensDirt.intensity = 0.45f;
            settings.volumetricFog.interiorDensityScale = 3.0f;
            settings.contactShadows.length = 0.6f;
        }

        void applySunset(render::RenderSettings& settings)
        {
            settings.lighting.directional = true;
            settings.sky = render::SkySettings{};
            settings.sky.sunAzimuth = math::radians(-120.0f);
            settings.sky.sunElevation = math::radians(6.0f);
            settings.sky.sunColor = {1.0f, 0.56f, 0.28f};
            settings.sky.sunIntensity = 3.2f;
            settings.sky.atmosphericSunTint = false;
            settings.sky.turbidity = 5.0f;
            settings.sky.skyIntensity = 1.2f;
            settings.sky.groundAlbedo = {0.2f, 0.16f, 0.13f};
            settings.sky.glowColor = {1.0f, 0.42f, 0.28f};
            settings.sky.glowIntensity = 0.3f;
            settings.sky.glowHeight = 0.15f;
            settings.sky.glowSpread = 2.0f;
            settings.sky.glowOpposite = 0.4f;
            settings.sky.zenithTint = {0.65f, 0.85f, 1.5f};
            settings.ibl = {.enabled = true, .diffuseIntensity = 1.3f, .specularIntensity = 1.0f};
            settings.fog = {.enabled = true, .color = {0.45f, 0.3f, 0.25f}, .density = 0.006f, .start = 15.0f,
                .maxAmount = 0.75f, .skyBlend = 0.85f};
            settings.bloom.strength = 0.06f;
            settings.volumetricFog.density = 0.01f;
            settings.volumetricFog.albedo = {1.0f, 0.86f, 0.72f};
            settings.volumetricFog.anisotropy = 0.6f;
            settings.volumetricFog.sunIntensity = 1.5f;
            settings.volumetricFog.interiorDensityScale = 3.0f;
            settings.autoExposure.targetLuminance = 0.12f;
            settings.autoExposure.minEv = -2.5f;
            settings.autoExposure.maxEv = 2.0f;
            settings.colorGrading.lift = {-0.01f, -0.004f, 0.0f};
            settings.colorGrading.gain = {1.04f, 1.0f, 0.94f};
            settings.colorGrading.contrast = 1.1f;
            settings.colorGrading.saturation = 0.95f;
            settings.colorGrading.shadowTint = {0.35f, 0.6f, 0.7f};
            settings.colorGrading.highlightTint = {1.0f, 0.72f, 0.45f};
            settings.colorGrading.splitToneStrength = 0.4f;
            settings.colorGrading.splitToneBalance = 0.4f;
        }

        void applyNight(render::RenderSettings& settings)
        {
            settings.lighting.directional = false;
            settings.sky = render::SkySettings{};
            settings.sky.sunElevation = math::radians(-12.0f);
            settings.sky.sunDisk = false;
            settings.sky.skyIntensity = 0.04f;
            settings.ibl = {.enabled = true, .diffuseIntensity = 0.4f, .specularIntensity = 0.5f};
            settings.autoExposure.targetLuminance = 0.05f;
            settings.autoExposure.minEv = -1.0f;
            settings.autoExposure.maxEv = 2.5f;
            settings.colorGrading.shadowTint = {0.3f, 0.45f, 0.8f};
            settings.colorGrading.splitToneStrength = 0.35f;
            settings.volumetricFog.density = 0.02f;
            settings.volumetricFog.albedo = {0.8f, 0.85f, 0.95f};
            settings.volumetricFog.anisotropy = 0.3f;
            settings.volumetricFog.localLightIntensity = 1.5f;
            settings.volumetricFog.ambientIntensity = 0.004f;
            settings.volumetricFog.interiorDensityScale = 1.5f;
            settings.lensDirt.intensity = 0.6f;
            settings.chromaticAberration.intensity = 1.6f;
            settings.screenSpaceReflections.maxRoughness = 0.6f;
            settings.screenSpaceReflections.maxDistance = 35.0f;
        }
    } //namespace

    void applyLookPreset(render::RenderSettings& settings, LookPreset preset)
    {
        applyGrade(settings);

        switch (preset)
        {
            case LookPreset::Day:
            {
                applyDay(settings);
                break;
            }
            case LookPreset::Evening:
            {
                applyEvening(settings);
                break;
            }
            case LookPreset::Sunset:
            {
                applySunset(settings);
                break;
            }
            case LookPreset::Night:
            {
                applyNight(settings);
                break;
            }
        }
    }

    LookPreset applyLevelLook(render::Renderer& renderer, const assets::Scene& scene)
    {
        LookPreset preset = LookPreset::Day;

        for (const assets::SceneNode& node : scene.nodes)
        {
            const std::string name = lowered(node.name);

            if (!name.starts_with(k_lookPrefix))
            {
                continue;
            }

            const std::string_view rest = std::string_view(name).substr(k_lookPrefix.size());

            if (rest.starts_with(lookPresetName(LookPreset::Evening)))
            {
                preset = LookPreset::Evening;
            }
            else if (rest.starts_with(lookPresetName(LookPreset::Sunset)))
            {
                preset = LookPreset::Sunset;
            }
            else if (rest.starts_with(lookPresetName(LookPreset::Night)))
            {
                preset = LookPreset::Night;
            }

            break;
        }

        applyLookPreset(renderer.settings(), preset);

        return preset;
    }

    std::string_view lookPresetName(LookPreset preset)
    {
        switch (preset)
        {
            case LookPreset::Day:
            {
                return "day";
            }
            case LookPreset::Evening:
            {
                return "evening";
            }
            case LookPreset::Sunset:
            {
                return "sunset";
            }
            case LookPreset::Night:
            {
                return "night";
            }
        }

        return "day";
    }
} //namespace game
