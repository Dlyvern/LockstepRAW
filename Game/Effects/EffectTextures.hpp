#ifndef LOCKSTEP_GAME_EFFECTS_EFFECT_TEXTURES_HPP
#define LOCKSTEP_GAME_EFFECTS_EFFECT_TEXTURES_HPP

#include <cstddef>
#include <cstdint>
#include <vector>

namespace game
{
    // Layout of the particle sprite atlas: k_effectAtlasColumns x k_effectAtlasRows cells of
    // k_effectCellSize pixels, frames numbered row-major from the top-left.
    inline constexpr uint32_t k_effectAtlasColumns{4};
    inline constexpr uint32_t k_effectAtlasRows{4};
    inline constexpr uint32_t k_effectCellSize{128};
    inline constexpr uint32_t k_effectAtlasSize{k_effectAtlasColumns * k_effectCellSize};

    // Frames of the atlas.
    // k_smokeFrame:  first of k_smokeFrames soft, noisy smoke/dust puffs (white, density in alpha).
    // k_dotFrame:    a soft round dot (motes, dust grains, embers).
    // k_sparkFrame:  a small hot core with a soft falloff (sparks; stretched along their motion).
    // k_paperFrame:  a torn, creased scrap of paper (off-white, faint print lines).
    // k_leafFrame:   a dry leaf (brown, a darker midrib).
    // k_chunkFrame:  first of k_chunkFrames irregular hard-edged debris chips, lit from above.
    // k_flashFrame:  first of k_flashFrames muzzle flashes seen from the front: a hot core with
    //                ragged petals (white-yellow centre, orange edges).
    // k_flashSideFrame: a muzzle flash seen from the side, for streaks: narrow at the muzzle
    //                (u = 0, the streak's tail), widest a third along, a ragged tip at u = 1.
    inline constexpr uint32_t k_smokeFrame{0};
    inline constexpr uint32_t k_smokeFrames{4};
    inline constexpr uint32_t k_dotFrame{4};
    inline constexpr uint32_t k_sparkFrame{5};
    inline constexpr uint32_t k_paperFrame{6};
    inline constexpr uint32_t k_leafFrame{7};
    inline constexpr uint32_t k_chunkFrame{8};
    inline constexpr uint32_t k_chunkFrames{4};
    inline constexpr uint32_t k_flashFrame{12};
    inline constexpr uint32_t k_flashFrames{3};
    inline constexpr uint32_t k_flashSideFrame{15};

    // Generates the particle sprite atlas procedurally (no art asset needed): sRGB colour, straight
    // alpha. Transparent texels keep the colour of their sprite so mips do not darken the edges.
    // Returns: k_effectAtlasSize * k_effectAtlasSize RGBA8 pixels, rows top to bottom.
    [[nodiscard]] std::vector<std::byte> effectAtlasPixels();

    // Generates a run of atlas frames side by side in one row (a texture of its own, e.g. for a
    // mesh drawn with a material).
    // firstFrame: the first frame.
    // frameCount: how many frames, left to right.
    // Returns:    (frameCount * k_effectCellSize) x k_effectCellSize RGBA8 pixels, like
    //             effectAtlasPixels().
    [[nodiscard]] std::vector<std::byte> effectStripPixels(uint32_t firstFrame, uint32_t frameCount);
} //namespace game

#endif //LOCKSTEP_GAME_EFFECTS_EFFECT_TEXTURES_HPP
