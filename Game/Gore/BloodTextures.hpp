#ifndef LOCKSTEP_GAME_GORE_BLOOD_TEXTURES_HPP
#define LOCKSTEP_GAME_GORE_BLOOD_TEXTURES_HPP

#include <cstddef>
#include <cstdint>
#include <vector>

namespace game
{
    // Which blood decal image to generate.
    // Splatter: an impact spray: a ragged centre, droplets and streaks thrown outwards.
    // Pool:     a thick, wet pool with a lumpy edge.
    // Drop:     a single drip with a few specks.
    enum class BloodTextureKind : uint8_t
    {
        Splatter = 0,
        Pool,
        Drop
    };

    // Generates a blood decal image procedurally (no art asset needed): sRGB colour, straight
    // alpha, darker where the blood is thick. The same arguments give the same pixels.
    // kind:    splatter, pool or drop.
    // size:    width and height in pixels.
    // seed:    picks the shape (different seeds give different splatters).
    // Returns: size * size RGBA8 pixels, rows top to bottom.
    [[nodiscard]] std::vector<std::byte> bloodTexture(BloodTextureKind kind, uint32_t size, uint32_t seed);
} //namespace game

#endif //LOCKSTEP_GAME_GORE_BLOOD_TEXTURES_HPP
