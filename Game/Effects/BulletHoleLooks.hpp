#ifndef LOCKSTEP_GAME_EFFECTS_BULLET_HOLE_LOOKS_HPP
#define LOCKSTEP_GAME_EFFECTS_BULLET_HOLE_LOOKS_HPP

#include <cstddef>
#include <cstdint>
#include <vector>

namespace game
{
    // What a bullet hole is punched into; each has its own decal look.
    // Concrete: a dark hole in a chipped, dusty crater with a faint scorch (the fallback).
    // Plaster:  plaster or drywall: a wide powdery spall of white gypsum around the hole.
    // Tile:     a hole in a chipped white glaze ring with cracks running out across the tile.
    // Wood:     a dark hole with pale torn splinters along the grain.
    // Metal:    a small clean hole in a bright, dented ring of bare steel (it glints) with chipped
    //           paint around it.
    // Glass:    a small crushed hole with a star of radial cracks and broken rings (glass that does
    //           not break; breakable panes shatter instead).
    // Fabric:   a small dark hole with frayed fibres (mattresses, sofas, curtains, bags).
    enum class HoleSurface : uint8_t
    {
        Concrete = 0,
        Plaster,
        Tile,
        Wood,
        Metal,
        Glass,
        Fabric,
        Count
    };

    // Number of HoleSurface kinds.
    inline constexpr std::size_t k_holeSurfaceCount{static_cast<std::size_t>(HoleSurface::Count)};

    // Side length of a hole texture in texels.
    inline constexpr uint32_t k_holeTextureSize{128};

    // How a hole on one surface is drawn.
    // sizeScale:  factor on the hole decal's size (cracks and spalls need room).
    // relief:     crater relief in UV units (render::MaterialShading::decalRelief); negative dents.
    // roughness:  material roughness.
    // metallic:   material metalness.
    struct HoleStyle
    {
        float sizeScale{1.0f};
        float relief{-0.03f};
        float roughness{0.85f};
        float metallic{0.0f};
    };

    // surface: a surface kind.
    // Returns: its lower-case name ("plaster").
    [[nodiscard]] const char* holeSurfaceName(HoleSurface surface);

    // surface: a surface kind.
    // Returns: how its holes are drawn.
    [[nodiscard]] HoleStyle holeStyle(HoleSurface surface);

    // Paints the decal texture of a hole procedurally: sRGB colour, straight alpha (coverage, which
    // also drives the decal relief), the hole at the centre.
    // surface: the surface kind.
    // Returns: k_holeTextureSize * k_holeTextureSize RGBA8 pixels, rows top to bottom.
    [[nodiscard]] std::vector<std::byte> holeTexturePixels(HoleSurface surface);
} //namespace game

#endif //LOCKSTEP_GAME_EFFECTS_BULLET_HOLE_LOOKS_HPP
