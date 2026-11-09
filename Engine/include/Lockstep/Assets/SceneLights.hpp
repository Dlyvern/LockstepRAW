#ifndef LOCKSTEP_ASSETS_SCENE_LIGHTS_HPP
#define LOCKSTEP_ASSETS_SCENE_LIGHTS_HPP

#include "Lockstep/Assets/Scene.hpp"
#include "Lockstep/Render/Lighting.hpp"

namespace lockstep::assets
{
    // glTF point and spot intensity (candela) per unit of renderer intensity. The renderer's
    // intensity I lights a white surface 1 m away to a radiance of I with the Lambert pi folded
    // in, while glTF's candela are luminous (683 lm/W), so a Blender point light of P watts
    // exported with the "Standard" lighting mode (P / 4pi * 683 cd) becomes P / (4 pi^2): the
    // same brightness it has in Cycles. A 200 W bulb gives about 5.
    inline constexpr float k_candelaPerRenderIntensity{683.0f * 3.14159265f};

    // Radiance below which a light without an authored range is treated as having ended, used to
    // derive its range from its intensity.
    inline constexpr float k_derivedRangeCutoff{0.05f};

    // Renderer intensity of a point or spot scene light (see k_candelaPerRenderIntensity).
    // light:   the scene light.
    // Returns: the intensity to put in a render::PointLight or render::SpotLight.
    [[nodiscard]] float renderIntensity(const SceneLight& light);

    // Range of a point or spot scene light: its authored range, or, when it has none (glTF's
    // unlimited range), the distance where its brightest colour channel falls below
    // k_derivedRangeCutoff, clamped to 0.5-50 m.
    // light:   the scene light.
    // Returns: the range in metres.
    [[nodiscard]] float renderRange(const SceneLight& light);

    // Converts a point scene light into a renderer light.
    // light:   the scene light (its type is not checked).
    // Returns: the point light at the light's world position.
    [[nodiscard]] render::PointLight toPointLight(const SceneLight& light);

    // Converts a spot scene light into a renderer light, shining along the transform's local -Z.
    // The inner cone is narrowed when needed so it stays inside the outer one. No shadows.
    // light:   the scene light (its type is not checked).
    // Returns: the spot light.
    [[nodiscard]] render::SpotLight toSpotLight(const SceneLight& light);
} //namespace lockstep::assets

#endif //LOCKSTEP_ASSETS_SCENE_LIGHTS_HPP
