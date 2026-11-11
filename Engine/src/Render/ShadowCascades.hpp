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
    // Near plane of local light shadow projections, in metres.
    inline constexpr float k_localShadowNear{0.05f};

    // Tiles of the shadow atlas: the sun's cascades first, then the local light tiles.
    inline constexpr uint32_t k_sunShadowTiles{k_maxShadowCascades};
    inline constexpr uint32_t k_maxShadowTiles{k_sunShadowTiles + k_maxLocalShadowTiles};

    // Atlas tiles a point light's shadow takes: one per cube face (+X, -X, +Y, -Y, +Z, -Z).
    inline constexpr uint32_t k_pointShadowFaces{6};

    // One rendered tile of the shadow atlas.
    // viewProjection:      world to the light's clip space, before placement in the atlas (culling).
    // atlasViewProjection: world to atlas clip space: the tile's part of the atlas (drawing and
    //                      lookups).
    // texelSize:           world-space size of one shadow texel (orthographic cascades), or its size
    //                      per metre of distance from the light (perspective tiles).
    struct ShadowTile
    {
        math::Mat4 viewProjection{1.0f};
        math::Mat4 atlasViewProjection{1.0f};
        float texelSize{0.0f};
    };

    // tile:    a tile index, below k_maxShadowTiles.
    // Returns: the tile's rectangle in atlas uv: xy the top-left corner, zw the size.
    [[nodiscard]] math::Vec4 shadowTileRect(uint32_t tile);

    // tile:       a tile index, below k_maxShadowTiles.
    // atlasWidth: width of the shadow atlas in texels (k_shadowAtlasWidth at the default size).
    // Returns:    the tile's side length in texels.
    [[nodiscard]] uint32_t shadowTileTexels(uint32_t tile, uint32_t atlasWidth);

    // tile:    a tile index, below k_maxShadowTiles.
    // Returns: the clip-space transform squeezing a full viewport into that tile.
    [[nodiscard]] math::Mat4 atlasTileTransform(uint32_t tile);

    // Fits stable (texel-snapped, rotation-invariant) cascades of the sun's shadow to the camera.
    // camera:      the frame's camera.
    // aspectRatio: viewport width / height.
    // direction:   unit direction sunlight travels in.
    // settings:    cascade distance and split distribution.
    // count:       cascades to fit, 1 to k_sunShadowTiles.
    // atlasWidth:  width of the shadow atlas in texels.
    // tiles:       receives the cascades in tiles 0..count-1.
    void fitSunCascades(const Camera& camera, float aspectRatio, const math::Vec3& direction,
        const ShadowSettings& settings, uint32_t count, uint32_t atlasWidth,
        std::array<ShadowTile, k_maxShadowTiles>& tiles);

    // Fits a spot light's perspective shadow into atlas tile `tile`.
    // light:      the spot light.
    // tile:       the atlas tile.
    // atlasWidth: width of the shadow atlas in texels.
    // Returns:    the tile.
    [[nodiscard]] ShadowTile fitSpotShadow(const SpotLight& light, uint32_t tile, uint32_t atlasWidth);

    // Fits one cube face of a point light's shadow into atlas tile `tile`. The face's field of view
    // is a little over 90 degrees, so the filter footprint of a lookup near a face edge stays inside
    // the face that the lookup picks.
    // light: the point light.
    // face:  the cube face, 0 to 5 (+X, -X, +Y, -Y, +Z, -Z).
    // tile:  the atlas tile.
    // atlasWidth: width of the shadow atlas in texels.
    // Returns: the tile.
    [[nodiscard]] ShadowTile fitPointShadowFace(const PointLight& light, uint32_t face, uint32_t tile,
        uint32_t atlasWidth);
} //namespace lockstep::render

#endif //LOCKSTEP_RENDER_SHADOW_CASCADES_HPP
