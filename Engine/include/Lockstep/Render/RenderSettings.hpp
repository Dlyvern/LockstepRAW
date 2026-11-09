#ifndef LOCKSTEP_RENDER_RENDER_SETTINGS_HPP
#define LOCKSTEP_RENDER_RENDER_SETTINGS_HPP

#include "Lockstep/Core/Math.hpp"

#include <cstdint>

namespace lockstep::render
{
    // Most shadow cascades the sun can use.
    inline constexpr uint32_t k_maxShadowCascades{4};

    // Which lights and shading terms lit materials use.
    // directional:      use the sun (the submitted directional light, or the sky's sun).
    // localLights:      use point and spot lights.
    // specular:         add specular reflections (direct and from the environment).
    // ambientColor:     linear RGB light reaching every surface from everywhere, on top of
    //                   image-based lighting. Off (0) by default; for scenes without a sky.
    // ambientIntensity: multiplier on `ambientColor`.
    struct LightingSettings
    {
        bool directional{true};
        bool localLights{true};
        bool specular{true};
        math::Vec3 ambientColor{1.0f, 1.0f, 1.0f};
        float ambientIntensity{0.0f};
    };

    // Procedural daylight sky (Preetham) and the sun in it. The sky is drawn behind everything and
    // lights the scene through image-based lighting. When no DirectionalLight is submitted in a
    // frame, the sun described here is the frame's directional light (casting cascaded shadows);
    // a submitted DirectionalLight replaces it and the sky follows that light's direction.
    // enabled:      draw the sky and provide its sun.
    // sunAzimuth:   compass angle of the sun in radians: 0 is towards -Z, pi/2 towards +X.
    // sunElevation: angle of the sun above the horizon in radians.
    // sunColor:     linear RGB colour of sunlight above the atmosphere.
    // sunIntensity: brightness of sunlight; 1 lights a white surface facing the sun to 1.
    // atmosphericSunTint: redden and dim the sun by the air it passes through (low sun is orange).
    // turbidity:    haziness of the air, 2 (clear) to 10 (hazy).
    // skyIntensity: brightness multiplier of the sky radiance.
    // groundAlbedo: linear RGB reflectance of the ground below the horizon (lights from below).
    // sunDisk:      draw the sun's disk in the sky.
    struct SkySettings
    {
        bool enabled{true};
        float sunAzimuth{math::radians(35.0f)};
        float sunElevation{math::radians(42.0f)};
        math::Vec3 sunColor{1.0f, 0.97f, 0.92f};
        float sunIntensity{2.6f};
        bool atmosphericSunTint{true};
        float turbidity{2.5f};
        float skyIntensity{1.0f};
        math::Vec3 groundAlbedo{0.25f, 0.24f, 0.22f};
        bool sunDisk{true};
    };

    // Image-based lighting from the sky: diffuse irradiance and prefiltered specular reflections,
    // with a split-sum BRDF lookup table. Needs the sky.
    // enabled:           light surfaces from the environment.
    // diffuseIntensity:  multiplier on environment diffuse light.
    // specularIntensity: multiplier on environment reflections.
    struct ImageBasedLightingSettings
    {
        bool enabled{true};
        float diffuseIntensity{1.0f};
        float specularIntensity{1.0f};
    };

    // Shadows: cascaded shadow maps for the sun in a shared atlas, whose last tile a
    // shadow-casting spot light takes over (the sun keeps the other cascades).
    // enabled:       render and sample shadow maps.
    // cascadeCount:  sun cascades, 1 to k_maxShadowCascades.
    // distance:      view distance the cascades cover, in metres. Beyond it the sun casts none.
    // splitLambda:   cascade split distribution, 0 uniform to 1 logarithmic.
    // depthBias:     slope-scaled depth bias rendered into the shadow map against acne, roughly in
    //                shadow texels of depth along the caster's slope. A change rebuilds the shadow
    //                materials (depth bias is pipeline state), so avoid changing it every frame.
    // normalBias:    offset of the shadow lookup along the surface normal against acne, in shadow
    //                texels.
    // debugCascades: tint the scene by the cascade each pixel reads.
    struct ShadowSettings
    {
        bool enabled{true};
        uint32_t cascadeCount{k_maxShadowCascades};
        float distance{70.0f};
        float splitLambda{0.8f};
        float depthBias{1.0f};
        float normalBias{1.5f};
        bool debugCascades{false};
    };

    // Exponential-squared distance fog, applied to lit materials.
    // enabled:   apply fog.
    // color:     linear RGB colour surfaces fade into.
    // density:   how quickly the fog thickens with distance, per metre.
    // start:     distance from the camera where fog begins, in metres.
    // maxAmount: upper bound of fog coverage in [0, 1], so far surfaces never vanish completely.
    struct FogSettings
    {
        bool enabled{false};
        math::Vec3 color{0.6f, 0.7f, 0.85f};
        float density{0.005f};
        float start{20.0f};
        float maxAmount{0.6f};
    };

    // Screen-space ambient occlusion from the depth buffer, at half resolution. Darkens only
    // ambient (environment) light, never direct light.
    // enabled:   compute and apply it.
    // radius:    world-space sampling radius in metres.
    // intensity: strength of the darkening.
    // power:     contrast curve applied to the result.
    // showOnly:  display the occlusion buffer instead of the scene.
    struct AmbientOcclusionSettings
    {
        bool enabled{true};
        float radius{0.6f};
        float intensity{1.0f};
        float power{1.5f};
        bool showOnly{false};
    };

    // Physically based bloom: a mip chain of the HDR scene blended back with a small weight.
    // enabled:  compute and apply it.
    // strength: blend weight of the blurred image, 0 to 1.
    struct BloomSettings
    {
        bool enabled{true};
        float strength{0.04f};
    };

    // Curve that maps HDR scene colour to the displayable range.
    // Clamp: no curve, values above 1 clip. Reinhard: soft, low contrast. Aces: filmic, contrasty.
    enum class Tonemapper : uint8_t
    {
        Clamp = 0,
        Reinhard,
        Aces
    };

    // HDR to display conversion of the final image.
    // enabled:   apply exposure and the curve; when false the scene colour is shown as is (clipped).
    // tonemapper: the curve.
    // exposure:  linear multiplier applied before the curve.
    // dither:    add sub-quantum noise against banding in dark gradients.
    struct TonemapSettings
    {
        bool enabled{true};
        Tonemapper tonemapper{Tonemapper::Aces};
        float exposure{1.0f};
        bool dither{true};
    };

    // Anti-aliasing of the final image.
    // fxaa:        run FXAA on the tonemapped image.
    // msaaSamples: samples per pixel of the lit scene pass (colour, ambient and depth), resolved
    //              before post-processing: 1 (off), 2, 4 or 8, lowered to what the GPU supports.
    //              A change rebuilds the frame's attachments.
    struct AntiAliasingSettings
    {
        bool fxaa{true};
        uint32_t msaaSamples{1};
    };

    // CPU-side visibility.
    // frustumCulling: skip draws whose bounds are outside the camera frustum (and, for shadows,
    //                 outside each cascade).
    struct CullingSettings
    {
        bool frustumCulling{true};
    };

    // Every renderer feature switch and tunable. Game sets its look once; the dev tools edit it live.
    struct RenderSettings
    {
        LightingSettings lighting{};
        SkySettings sky{};
        ImageBasedLightingSettings ibl{};
        ShadowSettings shadows{};
        FogSettings fog{};
        AmbientOcclusionSettings ambientOcclusion{};
        BloomSettings bloom{};
        TonemapSettings tonemap{};
        AntiAliasingSettings antiAliasing{};
        CullingSettings culling{};
    };
} //namespace lockstep::render

#endif //LOCKSTEP_RENDER_RENDER_SETTINGS_HPP
