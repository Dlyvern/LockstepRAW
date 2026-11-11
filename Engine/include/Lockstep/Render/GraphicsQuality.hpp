#ifndef LOCKSTEP_RENDER_GRAPHICS_QUALITY_HPP
#define LOCKSTEP_RENDER_GRAPHICS_QUALITY_HPP

#include "Lockstep/Render/RenderSettings.hpp"

#include <cstdint>
#include <optional>
#include <string_view>

namespace lockstep::render
{
    // Named graphics quality levels. Low is for weak GPUs and small VRAM (also for test runs), High
    // is the default look, Ultra adds resolution and samples on top of it.
    enum class GraphicsPreset : uint8_t
    {
        Low = 0,
        Medium,
        High,
        Ultra,
        Custom
    };

    // Smallest and largest render resolution scale.
    inline constexpr float k_minRenderScale{0.5f};
    inline constexpr float k_maxRenderScale{1.0f};

    // Most top mip levels dropped from textures at load time.
    inline constexpr uint32_t k_maxTextureMipDrop{3};

    // Smallest and largest shadow atlas width in texels (the height is half of it).
    inline constexpr uint32_t k_minShadowAtlasSize{1024};
    inline constexpr uint32_t k_maxShadowAtlasSize{8192};

    // Most lights GraphicsQuality::volumetricFogLights can allow.
    inline constexpr uint32_t k_maxVolumetricFogLights{16};

    // Most screen lights GraphicsQuality::screenLights can allow.
    inline constexpr uint32_t k_maxScreenLights{8};

    // Range of a set frame rate limit or cap, in frames per second.
    inline constexpr uint32_t k_minFrameRateLimit{20};
    inline constexpr uint32_t k_maxFrameRateLimit{1000};

    // How much rendering quality the GPU is asked for, separate from the look Game sets in
    // RenderSettings (colours, densities, grading). The Renderer renders RenderSettings limited by
    // this: a feature switched off here stays off whatever RenderSettings says, and step counts
    // and resolutions are scaled. Presets fill every field; individual fields may then be changed
    // (the preset becomes Custom in the dev tools and the settings file).
    // preset:              the preset the fields came from (Custom once one was changed).
    // renderScale:         size of the 3D scene relative to the window, k_minRenderScale to 1; the
    //                      result is upscaled (bilinear, sharpened with TAA) to the window.
    // textureMipDrop:      top mip levels skipped when a texture is created, 0 to
    //                      k_maxTextureMipDrop; each level quarters the texture's memory. Textures
    //                      are never cut below 128 texels on their longer side. Applies to textures
    //                      created afterwards (reload the level to change it).
    // antiAliasing:        anti-aliasing technique (see AntiAliasingMode).
    // shadowAtlasSize:     width of the shadow atlas in texels (power of two, k_minShadowAtlasSize
    //                      to k_maxShadowAtlasSize); 4096 gives sun cascade tiles of 1024 and local
    //                      light tiles of 512.
    // shadowCascades:      most sun cascades, 1 to k_maxShadowCascades.
    // shadowedLocalLights: most shadow-casting point and spot lights per frame, 0 to
    //                      k_maxShadowedLocalLights.
    // characterShadowCascades: most sun cascades skinned characters cast into, 0 to
    //                      k_maxShadowCascades (ShadowSettings::skinnedCasterCascades); fewer means
    //                      characters far from the camera lose their sun shadow.
    // volumetricFog:       allow volumetric fog.
    // volumetricFogResolution: multiplier on VolumetricFogSettings::resolutionScale.
    // volumetricFogLights: most point and spot lights (nearest the camera) that scatter in the
    //                      volumetric fog, 0 to k_maxVolumetricFogLights.
    // screenSpaceReflections: allow screen-space reflections.
    // ambientOcclusion:    allow ambient occlusion (GTAO).
    // contactShadows:      allow contact shadows.
    // effectSteps:         multiplier on the sample counts of volumetric fog, screen-space
    //                      reflections, ambient occlusion, contact shadows, parallax, motion blur
    //                      and depth of field (each still clamped to its own range).
    // parallax:            allow parallax occlusion mapping.
    // parallaxLocalShadows: parallax relief also shadows itself from point and spot lights (off:
    //                      from the sun only; each local light costs a height-map march).
    // surfaceDetail:       allow the micro-detail layer.
    // skinScattering:      allow subsurface scattering on skin materials (ShadingModel::Skin).
    // fabricSheen:         allow the sheen lobe on fabric materials (ShadingModel::Fabric).
    // characterSurfaces:   allow blood, dirt and rain soak on characters (CharacterSurface).
    // decalRelief:         allow decal relief (MaterialShading::decalRelief).
    // tubeLights:          shade tube lights as lines (off: points at their centre).
    // lightProfiles:       allow light emission profiles (LightProfile).
    // softShadows:         allow contact-hardening soft shadows (SoftShadowSettings; their tap
    //                      counts follow effectSteps).
    // irradianceVolume:    allow baked bounce light (IrradianceVolumeSettings).
    // shadowCasterTexels:  static casters smaller than this many shadow texels across are skipped
    //                      (ShadowDetailSettings::minCasterTexels, the larger of the two wins), 0 to 16.
    // motionBlur:          allow motion blur.
    // depthOfField:        allow depth of field.
    // lensEffects:         allow chromatic aberration and lens dirt.
    // bloom:               allow bloom.
    // lensFlares:          allow lens flares and sun glare (LensFlareSettings; need bloom).
    // bokeh:               depth of field blurs into aperture shapes with bright highlights
    //                      (DepthOfFieldSettings::apertureBlades, bokehHighlights; off: a plain disc).
    // planarReflections:   allow mirrors.
    // reflectionResolution: multiplier on PlanarReflectionSettings::resolutionScale.
    // scopeResolution:     most texels per side of the scope view, 256 to 1024.
    // particleBudget:      particles Game should allow alive at once (read by Game when it creates
    //                      its ParticleSystem).
    // decalBudget:         bullet holes and similar impact decals Game should keep at once (read by
    //                      Game every frame).
    // screenLights:        most glowing screens (monitors, TVs, vending machines) Game should turn
    //                      into lights per frame, 0 to k_maxScreenLights; they share the frame's
    //                      k_maxLocalLights with every other point and spot light.
    // waterRipples:        allow dripping water, splashes and ripple rings (read by Game).
    // volumetricClouds:    raymarch the clouds (CloudSettings::volumetric; off: the flat cloud layer
    //                      drawn with the sky); their steps follow effectSteps.
    // lensRain:            allow rain drops on the lens (LensRainSettings).
    // lightningShadows:    lightning flashes cast sun-like shadows (LightningSettings::castsShadows).
    // foliageWind:         allow foliage swaying in the wind (WindSettings).
    // windDebris:          allow litter and leaves blowing in the wind (read by Game).
    // cloth:               allow simulated cloth such as curtains (read by Game; off: they hang still).
    // frameRateLimit:      most frames per second the run loop renders, 0 for no limit, else 20 to
    //                      1000 (on top of the present mode; see Renderer::frameRateLimit()).
    struct GraphicsQuality
    {
        GraphicsPreset preset{GraphicsPreset::High};
        float renderScale{1.0f};
        uint32_t textureMipDrop{0};
        AntiAliasingMode antiAliasing{AntiAliasingMode::Taa};
        uint32_t shadowAtlasSize{4096};
        uint32_t shadowCascades{4};
        uint32_t shadowedLocalLights{4};
        uint32_t characterShadowCascades{2};
        bool volumetricFog{true};
        float volumetricFogResolution{0.7f};
        uint32_t volumetricFogLights{6};
        bool screenSpaceReflections{true};
        bool ambientOcclusion{true};
        bool contactShadows{true};
        float effectSteps{1.0f};
        bool parallax{true};
        bool parallaxLocalShadows{false};
        bool surfaceDetail{true};
        bool skinScattering{true};
        bool fabricSheen{true};
        bool characterSurfaces{true};
        bool decalRelief{true};
        bool tubeLights{true};
        bool lightProfiles{true};
        bool softShadows{true};
        bool irradianceVolume{true};
        float shadowCasterTexels{2.0f};
        bool motionBlur{true};
        bool depthOfField{true};
        bool lensEffects{true};
        bool bloom{true};
        bool lensFlares{true};
        bool bokeh{true};
        bool planarReflections{true};
        float reflectionResolution{1.0f};
        uint32_t scopeResolution{512};
        uint32_t particleBudget{20000};
        uint32_t decalBudget{160};
        uint32_t screenLights{4};
        bool waterRipples{true};
        bool volumetricClouds{true};
        bool lensRain{true};
        bool lightningShadows{true};
        bool foliageWind{true};
        bool windDebris{true};
        bool cloth{true};
        uint32_t frameRateLimit{0};
    };

    // preset:  a preset other than Custom.
    // Returns: the quality fields of that preset (Custom gives High's).
    [[nodiscard]] GraphicsQuality graphicsQualityPreset(GraphicsPreset preset);

    // Returns: the lowercase name of a preset ("low", "medium", "high", "ultra", "custom").
    // preset:  the preset.
    [[nodiscard]] const char* toString(GraphicsPreset preset);

    // name:    a preset name as toString() spells it (case-sensitive).
    // Returns: the preset, or nothing if the name is unknown.
    [[nodiscard]] std::optional<GraphicsPreset> parseGraphicsPreset(std::string_view name);

    // Returns: `quality` with every field clamped to its documented range.
    // quality: the quality to check.
    [[nodiscard]] GraphicsQuality clampedQuality(const GraphicsQuality& quality);

    // The settings the Renderer actually renders: `requested` with features `quality` switches off
    // turned off, sample counts scaled and capped, and the anti-aliasing mode, shadow cascades and
    // shadowed light count taken from `quality`.
    // requested: the settings Game and the dev tools set.
    // quality:   the quality limits.
    // Returns:   the limited settings.
    [[nodiscard]] RenderSettings limitedByQuality(const RenderSettings& requested, const GraphicsQuality& quality);
} //namespace lockstep::render

#endif //LOCKSTEP_RENDER_GRAPHICS_QUALITY_HPP
