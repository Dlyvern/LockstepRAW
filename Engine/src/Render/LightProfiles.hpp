#ifndef LOCKSTEP_RENDER_LIGHT_PROFILES_HPP
#define LOCKSTEP_RENDER_LIGHT_PROFILES_HPP

#include <cstdint>
#include <vector>

namespace lockstep::render
{
    // Width of one emission profile in the atlas: azimuth around the light's axis, -pi to pi.
    inline constexpr uint32_t k_lightProfileWidth{128};

    // Height of one emission profile: (1 - cosine of the angle from the light's axis) / 2, 0 to 1.
    inline constexpr uint32_t k_lightProfileHeight{64};

    // Builds the emission profile atlas pbr.frag samples (shaders/light_shapes.glsl): one R8 tile
    // per LightProfile other than None, stacked top to bottom in enum order, each
    // k_lightProfileWidth x k_lightProfileHeight, peak 1.
    // Returns: the texels, k_lightProfileWidth x (k_lightProfileHeight * k_lightProfileCount).
    [[nodiscard]] std::vector<uint8_t> lightProfileTexels();
} //namespace lockstep::render

#endif //LOCKSTEP_RENDER_LIGHT_PROFILES_HPP
