#ifndef LOCKSTEP_RENDER_RENDER_SETTINGS_HPP
#define LOCKSTEP_RENDER_RENDER_SETTINGS_HPP

#include "Lockstep/Core/Math.hpp"

#include <cstdint>

namespace lockstep::render
{
    // Which lights and shading terms lit materials use.
    // directional:      use the frame's directional light.
    // localLights:      use point and spot lights.
    // specular:         add specular highlights.
    // ambientColor:     linear RGB light reaching every surface from everywhere.
    // ambientIntensity: multiplier on `ambientColor`.
    struct LightingSettings
    {
        bool directional{true};
        bool localLights{true};
        bool specular{true};
        math::Vec3 ambientColor{1.0f, 1.0f, 1.0f};
        float ambientIntensity{0.1f};
    };

    // The single shadow map, cast by a shadow-casting spot light or else the directional light.
    // enabled:    render and sample the shadow map.
    // depthBias:  offset of the shadow lookup towards the light against shadow acne, in metres.
    // normalBias: offset of the shadow lookup along the surface normal against shadow acne, in
    //             shadow-map texels.
    struct ShadowSettings
    {
        bool enabled{true};
        float depthBias{0.03f};
        float normalBias{1.5f};
    };

    // Exponential-squared distance fog, applied to lit materials.
    // enabled:   apply fog.
    // color:     linear RGB colour surfaces fade into.
    // density:   how quickly the fog thickens with distance, per metre.
    // start:     distance from the camera where fog begins, in metres.
    // maxAmount: upper bound of fog coverage in [0, 1], so far surfaces never vanish completely.
    struct FogSettings
    {
        bool enabled{true};
        math::Vec3 color{0.0f, 0.0f, 0.0f};
        float density{0.05f};
        float start{0.0f};
        float maxAmount{1.0f};
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

    // Every renderer feature switch and tunable. Game sets its look once; the dev tools edit it live.
    struct RenderSettings
    {
        LightingSettings lighting{};
        ShadowSettings shadows{};
        FogSettings fog{};
        TonemapSettings tonemap{};
    };
} //namespace lockstep::render

#endif //LOCKSTEP_RENDER_RENDER_SETTINGS_HPP
