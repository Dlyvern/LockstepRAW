#ifndef LOCKSTEP_RENDER_CHARACTER_SURFACE_HPP
#define LOCKSTEP_RENDER_CHARACTER_SURFACE_HPP

#include "Lockstep/Core/Math.hpp"

#include <array>
#include <cstdint>

// Per-character surface layers (blood, dirt, rain) drawn over a skinned character's materials.
namespace lockstep::render
{
    // Most stains one character surface holds.
    inline constexpr uint32_t k_maxSurfaceStains{8};

    // Most character surfaces queued per frame; later ones are ignored.
    inline constexpr uint32_t k_maxCharacterSurfaces{64};

    // One stain on a character: a blot of blood and a wider smear of dirt around a point of the
    // body. It follows the body as it animates.
    // position:  the point in the skinned mesh's bind pose (model space before skinning; see
    //            anim::SkinnedInstance::bindPosition()).
    // radius:    metres the blood reaches; dirt reaches about 1.6 times as far. Blood runs further
    //            downwards (bind-pose -Y), like drips.
    // blood:     how much blood, 0 to 1.
    // freshness: 1 fresh (bright red, glossy and wet), 0 dried (dark brown, matte).
    // dirt:      how much dirt and grime, 0 to 1.
    struct SurfaceStain
    {
        math::Vec3 position{0.0f, 0.0f, 0.0f};
        float radius{0.06f};
        float blood{1.0f};
        float freshness{1.0f};
        float dirt{0.0f};
    };

    // What covers one character this frame, passed with its skinned draws (presentation only).
    // soak:       how wet with rain, 0 dry to 1 soaked: porous clothes darken, everything gets
    //             glossier. Outdoors in rain the shader also wets the body by the world wetness
    //             (RenderSettings::wetness), so this matters for a character who came inside.
    // grime:      general dirt over the whole body, 0 to 1 (patchy noise).
    // noiseSeed:  offsets the stain-edge and grime noise so characters differ.
    // stains:     the stains; the first `stainCount` are used.
    // stainCount: stains used, at most k_maxSurfaceStains.
    struct CharacterSurface
    {
        float soak{0.0f};
        float grime{0.0f};
        float noiseSeed{0.0f};
        std::array<SurfaceStain, k_maxSurfaceStains> stains{};
        uint32_t stainCount{0};
    };

    // A character surface queued for this frame (Renderer::submitCharacterSurface()); 0 is none.
    using CharacterSurfaceId = uint32_t;

    // No character surface: the draw shows its materials as they are.
    inline constexpr CharacterSurfaceId k_noCharacterSurface{0};
} //namespace lockstep::render

#endif //LOCKSTEP_RENDER_CHARACTER_SURFACE_HPP
