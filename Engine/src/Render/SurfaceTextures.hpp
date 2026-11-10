#ifndef LOCKSTEP_RENDER_SURFACE_TEXTURES_HPP
#define LOCKSTEP_RENDER_SURFACE_TEXTURES_HPP

#include "Lockstep/Render/PbrMaterial.hpp"

#include <cstdint>
#include <vector>

// Procedural textures the Renderer builds at startup: the generic micro-detail layers lit
// materials can ask for (SurfaceDetail) and the lens dirt bloom lights up. Generated on the CPU
// from seeded noise, so they are the same on every run and machine.
namespace lockstep::render
{
    // Side length of each surface detail texture, in texels (one repeat).
    inline constexpr uint32_t k_surfaceDetailSize{512};

    // Side length of the lens dirt texture, in texels (stretched over the screen).
    inline constexpr uint32_t k_lensDirtSize{512};

    // Builds one tiling micro-detail texture, RGBA8 linear, k_surfaceDetailSize squared, one level:
    // rg the tangent-space normal's xy (0.5 flat), b a roughness offset (0.5 none, 1 is +0.5),
    // a an albedo multiplier (0.5 is 1, 1 is 1.5).
    // kind:    the layer; must not be SurfaceDetail::None.
    // Returns: the texels, row by row.
    [[nodiscard]] std::vector<uint8_t> surfaceDetailTexels(SurfaceDetail kind);

    // Builds the lens dirt texture, RGBA8 linear, k_lensDirtSize squared, one level: rgb how much
    // bloom each point of the lens scatters (soft smudges, specks, a few smears), a unused.
    // Returns: the texels, row by row.
    [[nodiscard]] std::vector<uint8_t> lensDirtTexels();
} //namespace lockstep::render

#endif //LOCKSTEP_RENDER_SURFACE_TEXTURES_HPP
