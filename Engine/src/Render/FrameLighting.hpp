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
#include <span>
#include <vector>

namespace lockstep::render
{
    // One point or spot light as pbr.frag reads it (std430, 64 bytes).
    // positionRange:     xyz world position, w range.
    // color:             rgb colour times intensity, w its volumetric fog multiplier.
    // directionCosOuter: xyz spot direction, w cosine of the outer angle.
    // cosInnerKind:      x cosine of the inner angle, y 0 for point or 1 for spot, z its first
    //                    shadow atlas tile (-1 for none; a point light's six faces follow it), w the
    //                    shadow lookup's normal offset per metre of distance from the light.
    struct GpuLocalLight
    {
        float positionRange[4];
        float color[4];
        float directionCosOuter[4];
        float cosInnerKind[4];
    };

    // One interior ambient zone as pbr.frag reads it: xyz of the box's minimum and maximum corner.
    struct GpuAmbientZone
    {
        float min[4];
        float max[4];
    };

    // The per-frame lighting block the lit, sky and environment shaders read from their storage
    // buffer (std430). The layout must match shaders/frame_lighting.glsl.
    // shadowTileViewProjection: world to atlas clip space of each shadow atlas tile.
    // shadowTileRects:   each tile's rectangle in atlas uv (xy corner, zw size).
    // viewProjection:    the camera's world to clip matrix without TAA jitter (motion vectors).
    // previousViewProjection: last frame's viewProjection (equal to it on the first frame).
    // reflectionViewProjection: world to clip of the planar reflection view (mirrored camera,
    //                    oblique near plane on the mirror, horizontally flipped).
    // viewModelViewProjection: world to clip of viewmodel draws as rasterized (viewmodel
    //                    projection with TAA jitter, depth squeezed to the near end).
    // viewModelMotionViewProjection: the same without jitter (motion vectors).
    // previousViewModelMotionViewProjection: last frame's viewModelMotionViewProjection.
    // scopeViewProjection: world to clip of the scope view (ScopeView::objective, square).
    // reflectionCamera:  xyz the mirrored camera position, w 1 if a reflection is rendered.
    // reflectionPlane:   xyz the mirror's world normal (towards the camera), w minus its offset
    //                    (dot(xyz, p) + w is the signed distance of p).
    // reflectionTint:    rgb multiplier on the reflection.
    // cascadeTexelSize:  world size of a shadow texel per cascade.
    // ambient:           rgb flat ambient light.
    // sunDirection:      xyz direction sunlight travels, w 1 if there is a sun.
    // sunRadiance:       rgb sun colour times intensity.
    // fogColor:          rgb fog colour, w density.
    // fogParams:         x start, y 1 if enabled, z max amount, w sky blend (0 if the sky is off).
    // shadowParams:      x sun cascades in use (0 for no sun shadow), y unused, z normal bias in
    //                    texels, w 1 to tint by cascade. Depth bias is rendered into the shadow
    //                    map (hardware depth bias), not applied here.
    // localShadowParams: x PCF radius of local light shadows in texels, yzw unused.
    // iblParams:         x diffuse and y specular intensity, w 1 if image-based lighting is on.
    // skyA..skyE, skyZenith, skyGround: SkyCoefficients (zenith w is the radiance scale, ground w
    //                    is 1 if the sky is on).
    // skyGlow:           rgb sunset glow radiance at the horizon towards the sun, w glow height.
    // skyGlowShape:      x glow spread, y glow share opposite the sun, zw unused.
    // skyZenithTint:     rgb multiplier on the sky overhead, w unused.
    // sunDisk:           rgb radiance of the sun's disk, w cosine of its angular radius (0 hides it).
    // sceneDepthParams:  x, y the camera projection's [2][2] and [3][2] (view depth of a depth
    //                    value, as TAA computes it), for viewmodel motion vectors.
    // surfaceParallax:   x metres where parallax occlusion mapping has faded out (0 off), y its
    //                    fewest and z most height samples, w 1 for relief self-shadowing.
    // surfaceDetail:     x metres where the micro-detail layer has faded out (0 off), y its strength.
    // wetness:           x wetness amount (0 off), y puddle coverage, z puddle noise frequency per
    //                    metre, w ripple animation seconds.
    // wetnessShading:    x porous albedo multiplier, y share of roughness removed, z ripple
    //                    strength, w 1 to tint by wetness.
    // scopeCamera:       xyz the scope view's position, w 1 if a scope view is rendered.
    // scopeCameraForward: xyz the scope view's forward, w tan of half its field of view.
    // scopeCameraUp:     xyz the scope view's up, w the magnification.
    // scopeEyepiece:     xyz the eyepiece centre in the lens draw's space, w the eye relief.
    // scopeAxis:         xyz the scope axis in the lens draw's space, w the exit pupil radius
    //                    (0 draws no scope shadow).
    // scopeAxisUp:       xyz the scope's up in the lens draw's space, w the share of the scope
    //                    target's side the view is drawn in (from its first texel).
    // scopeReticle:      rgb the reticle line colour, w the fine line width.
    // scopeReticleShape: x post width, y post gap, z stadia spacing, w stadia half-length.
    // scopeIllumination: rgb the centre dot's radiance, w the dot radius.
    // scopeFog:          x fog density at the scope view's position, y height falloff, z base
    //                    height, w the distance it is applied over (0 for none).
    // scopeFogColor:     rgb the fog's ambient glow, w unused.
    // sightDirection:    xyz the holographic sight line in its window draw's space, w 1 if a
    //                    holographic sight is drawn.
    // sightReticle:      rgb the reticle's radiance, w the ring's angular radius.
    // sightShape:        x half the ring's angular width, y the dot's angular radius, zw unused.
    // counts:            x local lights, y 1 if specular is on.
    // lights:            the local lights.
    // zoneParams:        x interior ambient zones in use, y blend distance, z normal offset, w 1 to
    //                    tint by zone weight.
    // zoneLight:         x diffuse and y specular image-based light multiplier inside zones.
    // zoneFill:          rgb fill radiance inside zones.
    // zones:             the zones, nearest the camera first.
    struct GpuFrameLighting
    {
        math::Mat4 shadowTileViewProjection[k_maxShadowTiles]{};
        float shadowTileRects[k_maxShadowTiles][4];
        math::Mat4 viewProjection{1.0f};
        math::Mat4 previousViewProjection{1.0f};
        math::Mat4 reflectionViewProjection{1.0f};
        math::Mat4 viewModelViewProjection{1.0f};
        math::Mat4 viewModelMotionViewProjection{1.0f};
        math::Mat4 previousViewModelMotionViewProjection{1.0f};
        math::Mat4 scopeViewProjection{1.0f};
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
        float localShadowParams[4];
        float iblParams[4];
        float skyA[4];
        float skyB[4];
        float skyC[4];
        float skyD[4];
        float skyE[4];
        float skyZenith[4];
        float skyGround[4];
        float skyGlow[4];
        float skyGlowShape[4];
        float skyZenithTint[4];
        float sunDisk[4];
        float sceneDepthParams[4];
        float surfaceParallax[4];
        float surfaceDetail[4];
        float wetness[4];
        float wetnessShading[4];
        float scopeCamera[4];
        float scopeCameraForward[4];
        float scopeCameraUp[4];
        float scopeEyepiece[4];
        float scopeAxis[4];
        float scopeAxisUp[4];
        float scopeReticle[4];
        float scopeReticleShape[4];
        float scopeIllumination[4];
        float scopeFog[4];
        float scopeFogColor[4];
        float sightDirection[4];
        float sightReticle[4];
        float sightShape[4];
        uint32_t counts[4];
        GpuLocalLight lights[k_maxLocalLights];
        float zoneParams[4];
        float zoneLight[4];
        float zoneFill[4];
        GpuAmbientZone zones[k_maxFrameAmbientZones];
    };

    static_assert(sizeof(GpuLocalLight) == 64);
    static_assert(sizeof(GpuAmbientZone) == 32);
    static_assert(sizeof(GpuFrameLighting) == k_maxShadowTiles * 80 + 7 * 64 + 43 * 16 + k_maxLocalLights * 64
        + 3 * 16 + k_maxFrameAmbientZones * 32);

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
    // lighting:       the block uploaded to the shaders.
    // hasSun:         the frame has a sun (submitted, or the sky's).
    // cascadeCount:   sun cascades rendered, in tiles 0..cascadeCount-1.
    // localTileCount: local light tiles rendered, in tiles k_sunShadowTiles onwards.
    // shadowedLocalLights: local lights casting a shadow this frame.
    // tiles:          every atlas tile's projection.
    // casterSpheres:  per local light tile, xyz the light's position and w its range (casters
    //                 outside it are skipped).
    struct PreparedLighting
    {
        GpuFrameLighting lighting{};
        bool hasSun{false};
        uint32_t cascadeCount{0};
        uint32_t localTileCount{0};
        uint32_t shadowedLocalLights{0};
        std::array<ShadowTile, k_maxShadowTiles> tiles{};
        std::array<math::Vec4, k_maxLocalShadowTiles> casterSpheres{};

        // tile:    an atlas tile index.
        // Returns: true if the tile is rendered this frame.
        [[nodiscard]] bool isTileActive(uint32_t tile) const;
    };

    // What prepareLighting() reads besides the lights and settings.
    // camera:         the frame's camera (cascade fitting, shadow and zone priority).
    // aspectRatio:    viewport width / height.
    // viewProjection: the camera's unjittered world-to-clip matrix (which shadowed local lights
    //                 reach into the view).
    // zones:          every interior ambient zone; the nearest are packed.
    // shadowAtlasWidth: width of the shadow atlas in texels (texel snapping and sizes).
    struct LightingView
    {
        const Camera& camera;
        float aspectRatio{1.0f};
        math::Mat4 viewProjection{1.0f};
        std::span<const AmbientZone> zones;
        uint32_t shadowAtlasWidth{4096};
    };

    // Packs a frame's lights, sky, interior zones and settings into the GPU block and fits the
    // shadow atlas: the sun's cascades in the sun tiles, shadow-casting local lights whose range
    // reaches into the view, nearest first, in the local tiles while they and the light budget last.
    // lights:   the frame's submitted lights.
    // settings: the renderer settings.
    // view:     camera and zones.
    // Returns:  the prepared lighting.
    [[nodiscard]] PreparedLighting prepareLighting(const FrameLights& lights, const RenderSettings& settings,
        const LightingView& view);
} //namespace lockstep::render

#endif //LOCKSTEP_RENDER_FRAME_LIGHTING_HPP
