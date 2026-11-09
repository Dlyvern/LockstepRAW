#ifndef LOCKSTEP_RENDER_SHADOW_CASCADES_HPP
#define LOCKSTEP_RENDER_SHADOW_CASCADES_HPP

#include "Lockstep/Core/Math.hpp"
#include "Lockstep/Render/Camera.hpp"
#include "Lockstep/Render/Lighting.hpp"
#include "Lockstep/Render/RenderSettings.hpp"

#include <array>
#include <cstdint>

namespace lockstep::render
{
    // Tiles in the 2x2 shadow atlas.
    inline constexpr uint32_t k_maxShadowTiles{4};

    // One rendered tile of the shadow atlas.
    // viewProjection:      world to the light's clip space, before placement in the atlas (culling).
    // atlasViewProjection: world to atlas clip space: the tile's quarter of the atlas (drawing and
    //                      lookups).
    // texelSize:           world-space size of one shadow texel (orthographic cascades only).
    struct ShadowTile
    {
        math::Mat4 viewProjection{1.0f};
        math::Mat4 atlasViewProjection{1.0f};
        float texelSize{0.0f};
    };

    // tile:    a tile index, 0 to k_maxShadowTiles - 1 (x = tile % 2, y = tile / 2).
    // Returns: the clip-space transform squeezing a full viewport into that tile.
    [[nodiscard]] math::Mat4 atlasTileTransform(uint32_t tile);

    // Fits stable (texel-snapped, rotation-invariant) cascades of the sun's shadow to the camera.
    // camera:      the frame's camera.
    // aspectRatio: viewport width / height.
    // direction:   unit direction sunlight travels in.
    // settings:    cascade distance and split distribution.
    // count:       cascades to fit, 1 to k_maxShadowTiles.
    // tiles:       receives the cascades in tiles 0..count-1.
    void fitSunCascades(const Camera& camera, float aspectRatio, const math::Vec3& direction,
        const ShadowSettings& settings, uint32_t count, std::array<ShadowTile, k_maxShadowTiles>& tiles);

    // Fits a spot light's perspective shadow into atlas tile `tile`.
    // light: the spot light.
    // tile:  the atlas tile.
    // Returns: the tile.
    [[nodiscard]] ShadowTile fitSpotShadow(const SpotLight& light, uint32_t tile);
} //namespace lockstep::render

#endif //LOCKSTEP_RENDER_SHADOW_CASCADES_HPP
