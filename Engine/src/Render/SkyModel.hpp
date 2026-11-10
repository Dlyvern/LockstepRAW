#ifndef LOCKSTEP_RENDER_SKY_MODEL_HPP
#define LOCKSTEP_RENDER_SKY_MODEL_HPP

#include "Lockstep/Core/Math.hpp"
#include "Lockstep/Render/RenderSettings.hpp"

namespace lockstep::render
{
    // The Preetham sky for one sun position, in the form sky_model.glsl evaluates it.
    // perez*:       Perez distribution coefficients A..E; x for luminance Y, y and z for the x and y
    //               chromaticities.
    // zenith:       zenith Y, x, y, each divided by its Perez function at the zenith.
    // radianceScale: luminance to scene radiance (includes SkySettings::skyIntensity and the fade
    //               to night).
    // ground:       radiance of the ground below the horizon.
    // glow:         radiance of the sunset glow at the horizon towards the sun (SkySettings glow
    //               colour and intensity, scaled by skyIntensity and the fade to night).
    struct SkyCoefficients
    {
        math::Vec3 perezA{};
        math::Vec3 perezB{};
        math::Vec3 perezC{};
        math::Vec3 perezD{};
        math::Vec3 perezE{};
        math::Vec3 zenith{};
        float radianceScale{0.0f};
        math::Vec3 ground{};
        math::Vec3 glow{};
    };

    // azimuth:   compass angle in radians, 0 towards -Z, pi/2 towards +X.
    // elevation: angle above the horizon in radians.
    // Returns:   the unit direction sunlight travels in (from the sun towards the scene).
    [[nodiscard]] math::Vec3 sunTravelDirection(float azimuth, float elevation);

    // Colour times intensity of the sky's sun at the ground, reddened and dimmed by the air
    // it crosses when SkySettings::atmosphericSunTint is on.
    // sky:     the sky settings.
    // Returns: linear RGB sun radiance scale (as DirectionalLight colour * intensity).
    [[nodiscard]] math::Vec3 skySunRadiance(const SkySettings& sky);

    // Evaluates the sky model for a sun position.
    // sky:         the sky settings.
    // toSun:       unit direction towards the sun.
    // sunRadiance: the sun's colour times intensity, which lights the ground.
    // Returns:     the coefficients.
    [[nodiscard]] SkyCoefficients computeSky(const SkySettings& sky, const math::Vec3& toSun,
        const math::Vec3& sunRadiance);
} //namespace lockstep::render

#endif //LOCKSTEP_RENDER_SKY_MODEL_HPP
