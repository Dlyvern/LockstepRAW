#ifndef LOCKSTEP_RENDER_FRAME_LIGHTING_HPP
#define LOCKSTEP_RENDER_FRAME_LIGHTING_HPP

#include "Lockstep/Core/Math.hpp"
#include "Lockstep/Render/Camera.hpp"
#include "Lockstep/Render/Lighting.hpp"
#include "Lockstep/Render/RenderSettings.hpp"

#include "ShadowCascades.hpp"

#include <array>
#include <cstdint>
#include <optional>
#include <vector>

namespace lockstep::render
{
    // One point or spot light as pbr.frag reads it (std430, 64 bytes).
    // positionRange:     xyz world position, w range.
    // color:             rgb colour times intensity.
    // directionCosOuter: xyz spot direction, w cosine of the outer angle.
    // cosInnerKind:      x cosine of the inner angle, y 0 for point or 1 for spot.
    struct GpuLocalLight
    {
        float positionRange[4];
        float color[4];
        float directionCosOuter[4];
        float cosInnerKind[4];
    };

    // The per-frame lighting block the lit, sky and environment shaders read from their storage
    // buffer (std430). The layout must match shaders/frame_lighting.glsl.
    // shadowTileViewProjection: world to atlas clip space of each shadow atlas tile.
    // viewProjection:    the camera's world to clip matrix without TAA jitter (motion vectors).
    // previousViewProjection: last frame's viewProjection (equal to it on the first frame).
    // reflectionViewProjection: world to clip of the planar reflection view (mirrored camera,
    //                    oblique near plane on the mirror, horizontally flipped).
    // reflectionCamera:  xyz the mirrored camera position, w 1 if a reflection is rendered.
    // reflectionPlane:   xyz the mirror's world normal (towards the camera), w minus its offset
    //                    (dot(xyz, p) + w is the signed distance of p).
    // reflectionTint:    rgb multiplier on the reflection.
    // cascadeTexelSize:  world size of a shadow texel per cascade.
    // ambient:           rgb flat ambient light.
    // sunDirection:      xyz direction sunlight travels, w 1 if there is a sun.
    // sunRadiance:       rgb sun colour times intensity.
    // fogColor:          rgb fog colour, w density.
    // fogParams:         x start, y 1 if enabled, z max amount.
    // shadowParams:      x sun cascades in use (0 for no sun shadow), y unused, z normal bias in
    //                    texels, w 1 to tint by cascade. Depth bias is rendered into the shadow
    //                    map (hardware depth bias), not applied here.
    // spotShadowParams:  x index of the shadow-casting spot light among the local lights (-1 for
    //                    none), y normal bias per metre of distance, z unused, w its atlas tile.
    // iblParams:         x diffuse and y specular intensity, w 1 if image-based lighting is on.
    // skyA..skyE, skyZenith, skyGround: SkyCoefficients (zenith w is the radiance scale, ground w
    //                    is 1 if the sky is on).
    // sunDisk:           rgb radiance of the sun's disk, w cosine of its angular radius (0 hides it).
    // counts:            x local lights, y 1 if specular is on.
    struct GpuFrameLighting
    {
        math::Mat4 shadowTileViewProjection[k_maxShadowTiles]{};
        math::Mat4 viewProjection{1.0f};
        math::Mat4 previousViewProjection{1.0f};
        math::Mat4 reflectionViewProjection{1.0f};
        float reflectionCamera[4];
        float reflectionPlane[4];
        float reflectionTint[4];
        float cascadeTexelSize[4];
        float ambient[4];
        float sunDirection[4];
        float sunRadiance[4];
        float fogColor[4];
        float fogParams[4];
        float shadowParams[4];
        float spotShadowParams[4];
        float iblParams[4];
        float skyA[4];
        float skyB[4];
        float skyC[4];
        float skyD[4];
        float skyE[4];
        float skyZenith[4];
        float skyGround[4];
        float sunDisk[4];
        uint32_t counts[4];
        GpuLocalLight lights[k_maxLocalLights];
    };

    static_assert(sizeof(GpuLocalLight) == 64);
    static_assert(sizeof(GpuFrameLighting) == k_maxShadowTiles * 64 + 3 * 64 + 21 * 16 + k_maxLocalLights * 64);

    // The lights submitted for one frame.
    // directional: the directional light, if one was submitted.
    // points:      point lights, at most k_maxLocalLights together with `spots`.
    // spots:       spot lights.
    // dropped:     lights rejected because the limit was reached.
    struct FrameLights
    {
        std::optional<DirectionalLight> directional;
        std::vector<PointLight> points;
        std::vector<SpotLight> spots;
        uint32_t dropped{0};

        // Returns: true if another point or spot light still fits.
        [[nodiscard]] bool hasRoom() const;

        // Forgets every light, ready for the next frame.
        void clear();
    };

    // Everything the shadow pass and the lit pass need for one frame.
    // lighting:     the block uploaded to the shaders.
    // hasSun:       the frame has a sun (submitted, or the sky's).
    // cascadeCount: sun cascades rendered, in tiles 0..cascadeCount-1.
    // spotTile:     the tile a spot light's shadow uses, if any.
    // tiles:        every atlas tile's projection.
    struct PreparedLighting
    {
        GpuFrameLighting lighting{};
        bool hasSun{false};
        uint32_t cascadeCount{0};
        std::optional<uint32_t> spotTile;
        std::array<ShadowTile, k_maxShadowTiles> tiles{};

        // Returns: number of atlas tiles rendered this frame.
        [[nodiscard]] uint32_t tileCount() const;
    };

    // Packs a frame's lights, sky and settings into the GPU block and fits the shadow atlas: the
    // first shadow-casting spot light takes the last tile, the sun's cascades the rest.
    // lights:      the frame's submitted lights.
    // settings:    the renderer settings.
    // camera:      the frame's camera (cascade fitting).
    // aspectRatio: viewport width / height.
    // Returns:     the prepared lighting.
    [[nodiscard]] PreparedLighting prepareLighting(const FrameLights& lights, const RenderSettings& settings,
        const Camera& camera, float aspectRatio);
} //namespace lockstep::render

#endif //LOCKSTEP_RENDER_FRAME_LIGHTING_HPP
