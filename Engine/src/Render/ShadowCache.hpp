#ifndef LOCKSTEP_RENDER_SHADOW_CACHE_HPP
#define LOCKSTEP_RENDER_SHADOW_CACHE_HPP

#include "Lockstep/Core/Math.hpp"

#include "ShadowCascades.hpp"

#include <array>
#include <cstdint>

namespace lockstep::render
{
    struct RendererState;

    // A far sun cascade kept from the frame it was fitted in, so it is re-rendered only every few
    // frames (see holdFarCascades()).
    // tile:           the cascade as fitted then.
    // cameraPosition: camera position it was fitted for.
    // cameraForward:  camera forward direction it was fitted for.
    // sunTravel:      sun direction it was fitted for.
    // isValid:        a cascade is held.
    struct HeldCascade
    {
        ShadowTile tile{};
        math::Vec3 cameraPosition{0.0f, 0.0f, 0.0f};
        math::Vec3 cameraForward{0.0f, 0.0f, -1.0f};
        math::Vec3 sunTravel{0.0f, -1.0f, 0.0f};
        bool isValid{false};
    };

    // What each tile of the persistent shadow atlas holds, so the shadow pass re-renders only the
    // tiles whose light or casters changed.
    // tileKeys:      per tile, the caster key (FrameDrawLists::shadowKeys) the tile was last
    //                rendered with.
    // tileValid:     per tile, the tile holds that render.
    // cascades:      far sun cascades being held.
    // depthBias:     the shadow depth bias the tiles were rendered with.
    // isInvalidated: everything must be re-rendered (meshes or materials were destroyed, so a
    //                caster's identity may now name other geometry).
    // drawCalls:     draw calls the shadow pass recorded last frame (an instanced batch counts once).
    struct ShadowCache
    {
        std::array<uint64_t, k_maxShadowTiles> tileKeys{};
        std::array<bool, k_maxShadowTiles> tileValid{};
        std::array<HeldCascade, k_sunShadowTiles> cascades{};
        float depthBias{0.0f};
        bool isInvalidated{true};
        uint32_t drawCalls{0};

        // Forgets every cached tile and held cascade.
        void invalidate();
    };

    // Keeps the far sun cascades (from the third on) of an earlier frame instead of the ones
    // prepareLighting() just fitted, refreshing each only every other frame (staggered), and
    // sooner when the camera turned or moved enough that the held cascade's casters (culled to the
    // view) or its coverage would fall behind, or when the sun or the cascade size changed. Patches
    // the prepared tiles and the GPU lighting block, so culling, drawing and lookups all use the
    // held matrices. Call right after prepareLighting().
    // state: the renderer state with prepared lighting and camera set.
    void holdFarCascades(RendererState& state);
} //namespace lockstep::render

#endif //LOCKSTEP_RENDER_SHADOW_CACHE_HPP
