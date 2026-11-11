#ifndef LOCKSTEP_RENDER_CLOUD_NOISE_HPP
#define LOCKSTEP_RENDER_CLOUD_NOISE_HPP

#include <cstdint>
#include <vector>

namespace lockstep::render
{
    // Side length in texels of the cloud noise texture.
    inline constexpr uint32_t k_cloudNoiseSize{128};

    // Generates the tiling cloud noise the sky and cloud shaders read (shaders/clouds.glsl): r broad
    // cloud shapes (fractal noise over billowy cells), g a medium octave band, b fine detail for
    // eroding edges, a cells. Every channel tiles and spans 0 to 1.
    // Returns: k_cloudNoiseSize * k_cloudNoiseSize RGBA8 texels, rows top to bottom.
    [[nodiscard]] std::vector<uint8_t> cloudNoiseTexels();
} //namespace lockstep::render

#endif //LOCKSTEP_RENDER_CLOUD_NOISE_HPP
