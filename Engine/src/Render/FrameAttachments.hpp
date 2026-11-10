#ifndef LOCKSTEP_RENDER_FRAME_ATTACHMENTS_HPP
#define LOCKSTEP_RENDER_FRAME_ATTACHMENTS_HPP

#include <Vela/Graphics/RenderTypes.hpp>

#include <cstdint>
#include <string_view>

namespace lockstep::render
{
    // Names, formats and sizes of the render graph attachments the Renderer's frame passes share.
    // environment:   the sky as an octahedral atlas: prefiltered specular tiles and an irradiance
    //                tile (layout in shaders/octahedral.glsl), rebuilt every frame.
    // shadowMap:     the shadow atlas: sun cascades and local light tiles (see k_shadowAtlasWidth).
    // color:         HDR direct light, emissive, sky and transparent surfaces (lit scene pass).
    // ambientLit:    HDR environment / ambient light of opaque surfaces as the lit pass wrote it
    //                (diffuse and environment specular together).
    // ambient:       ambientLit after the indirect composite (ambient occlusion multi-bounce,
    //                specular occlusion, screen-space reflections), divided by the occlusion
    //                visibility that later passes multiply it by (shaders/scene_hdr.glsl).
    // surfaceNormal: lit pass G-buffer: xy octahedral view-space normal, z roughness, w one minus
    //                the share of `color` that is direct sunlight (shaders/surface_buffer.glsl).
    // surfaceSpecular: lit pass G-buffer: rgb the environment specular light inside ambientLit,
    //                w one minus the signed specular weight (negative for metals).
    // depth:         the scene depth buffer.
    // velocity:      per-pixel motion of the lit scene: xy screen motion since last frame in uv
    //                units (current minus previous), z the surface's view depth last frame (0 when
    //                unknown), for TAA.
    // reflection*:   the planar reflection view: colour (lit, ambient included) and the
    //                ambient, velocity and depth targets it needs to share the lit pass's
    //                pipelines; reduced resolution.
    // scope*:        the scope view (Renderer::submitScope()): colour (lit, ambient included,
    //                alpha 0 where only sky is seen) and the ambient, velocity, G-buffer and depth
    //                targets it needs to share the lit pass's pipelines; square, fixed size.
    // *Msaa:         multisampled colour, ambient, velocity and depth of the lit scene pass when MSAA is on,
    //                resolved into color, ambient and depth (depth takes sample zero).
    // fogScatter:    volumetric fog of this frame, reduced resolution: rgb light scattered towards
    //                the camera, a transmittance, both from the camera to the pixel's surface.
    // fogResolved:   fogScatter blended with last frame's (persistent, double-buffered by Vela);
    //                what the fog composite upsamples into color and ambient.
    // occlusion:     raw ambient occlusion (GTAO) and contact shadows, half resolution: r diffuse
    //                visibility, g specular visibility, b the multiplier contact shadows leave on
    //                the colour target.
    // occlusionBlur: depth-aware blurred occlusion, half resolution; what composition reads.
    // ssrHit:        screen-space reflection rays, half resolution: xy hit uv, z confidence (0 for
    //                none), w roughness.
    // ssrSource:     the lit HDR scene at half resolution, kept across frames; what reflection rays
    //                fetch (last frame's, reprojected).
    // ssrResolved:   reflected radiance (rgb, premultiplied by confidence) and confidence (a),
    //                accumulated over frames (persistent, double-buffered by Vela), half resolution.
    // bloomDown*/bloomUp*: the bloom mip chain; bloomUp1 is the result.
    //                bloomUp1's alpha is the exposure multiplier tonemapping applies (auto
    //                exposure, or 1), written by its pass from the exposure attachment.
    // luminance:     auto exposure: weighted log luminance of the scene on a small fixed grid.
    // luminanceReduced: the same, averaged into a smaller grid.
    // exposure:      1x1, kept across frames: the adapted exposure in stops (r) and this frame's
    //                target (g); alpha 0 until the first frame wrote it.
    // ldr:           the tonemapped and graded image, linear, before anti-aliasing.
    // taaHistory:    the TAA result, kept across frames (persistent, double-buffered by Vela):
    //                rgb the accumulated image, a this pixel's view depth (0 marks invalid).
    // display:       the anti-aliased, dithered image that is presented; Engine overlays draw on it.
    inline constexpr std::string_view k_environmentAttachment{"environment"};
    inline constexpr std::string_view k_shadowMapAttachment{"shadowMap"};
    inline constexpr std::string_view k_sceneColorAttachment{"color"};
    inline constexpr std::string_view k_sceneAmbientAttachment{"ambient"};
    inline constexpr std::string_view k_sceneAmbientLitAttachment{"ambientLit"};
    inline constexpr std::string_view k_surfaceNormalAttachment{"surfaceNormal"};
    inline constexpr std::string_view k_surfaceSpecularAttachment{"surfaceSpecular"};
    inline constexpr std::string_view k_surfaceNormalMsaaAttachment{"surfaceNormalMsaa"};
    inline constexpr std::string_view k_surfaceSpecularMsaaAttachment{"surfaceSpecularMsaa"};
    inline constexpr std::string_view k_reflectionSurfaceNormalAttachment{"reflectionSurfaceNormal"};
    inline constexpr std::string_view k_reflectionSurfaceSpecularAttachment{"reflectionSurfaceSpecular"};
    inline constexpr std::string_view k_ssrHitAttachment{"ssrHit"};
    inline constexpr std::string_view k_ssrSourceAttachment{"ssrSource"};
    inline constexpr std::string_view k_ssrResolvedAttachment{"ssrResolved"};
    inline constexpr std::string_view k_sceneDepthAttachment{"depth"};
    inline constexpr std::string_view k_sceneVelocityAttachment{"velocity"};
    inline constexpr std::string_view k_sceneVelocityMsaaAttachment{"velocityMsaa"};
    inline constexpr std::string_view k_reflectionColorAttachment{"reflectionColor"};
    inline constexpr std::string_view k_reflectionAmbientAttachment{"reflectionAmbient"};
    inline constexpr std::string_view k_reflectionVelocityAttachment{"reflectionVelocity"};
    inline constexpr std::string_view k_reflectionDepthAttachment{"reflectionDepth"};
    inline constexpr std::string_view k_scopeColorAttachment{"scopeColor"};
    inline constexpr std::string_view k_scopeAmbientAttachment{"scopeAmbient"};
    inline constexpr std::string_view k_scopeVelocityAttachment{"scopeVelocity"};
    inline constexpr std::string_view k_scopeSurfaceNormalAttachment{"scopeSurfaceNormal"};
    inline constexpr std::string_view k_scopeSurfaceSpecularAttachment{"scopeSurfaceSpecular"};
    inline constexpr std::string_view k_scopeDepthAttachment{"scopeDepth"};
    inline constexpr std::string_view k_taaHistoryAttachment{"taaHistory"};
    inline constexpr std::string_view k_sceneColorMsaaAttachment{"colorMsaa"};
    inline constexpr std::string_view k_sceneAmbientMsaaAttachment{"ambientMsaa"};
    inline constexpr std::string_view k_sceneDepthMsaaAttachment{"depthMsaa"};
    inline constexpr std::string_view k_fogScatterAttachment{"fogScatter"};
    inline constexpr std::string_view k_fogResolvedAttachment{"fogResolved"};
    inline constexpr std::string_view k_occlusionAttachment{"occlusion"};
    inline constexpr std::string_view k_occlusionBlurAttachment{"occlusionBlur"};
    inline constexpr std::string_view k_luminanceAttachment{"luminance"};
    inline constexpr std::string_view k_luminanceReducedAttachment{"luminanceReduced"};
    inline constexpr std::string_view k_exposureAttachment{"exposure"};
    inline constexpr std::string_view k_ldrAttachment{"ldr"};
    inline constexpr std::string_view k_displayAttachment{"display"};

    // Bloom chain: k_bloomLevels downsampled levels (each half the previous, starting at half
    // resolution) and k_bloomLevels - 1 upsampled ones.
    inline constexpr uint32_t k_bloomLevels{5};
    inline constexpr std::string_view k_bloomDownAttachments[k_bloomLevels]{"bloomDown1", "bloomDown2",
        "bloomDown3", "bloomDown4", "bloomDown5"};
    inline constexpr std::string_view k_bloomUpAttachments[k_bloomLevels - 1]{"bloomUp1", "bloomUp2",
        "bloomUp3", "bloomUp4"};

    // Side lengths of the auto exposure luminance grids; the first is a multiple of the second.
    inline constexpr uint32_t k_luminanceGridSize{64};
    inline constexpr uint32_t k_luminanceReducedSize{8};

    inline constexpr vela::graphics::TextureFormat k_sceneColorFormat{vela::graphics::TextureFormat::RGBA16Float};
    inline constexpr vela::graphics::TextureFormat k_depthFormat{vela::graphics::TextureFormat::Depth32Float};
    inline constexpr vela::graphics::TextureFormat k_occlusionFormat{vela::graphics::TextureFormat::RGBA8Unorm};
    inline constexpr vela::graphics::TextureFormat k_displayFormat{vela::graphics::TextureFormat::RGBA16Float};

    // Shadow atlas layout at its default size (k_shadowAtlasWidth): the sun's 2x2 cascade tiles of
    // k_shadowTileSize texels fill the left half, local light tiles of k_localShadowTileSize texels
    // (a 4x4 grid) the right half. GraphicsQuality::shadowAtlasSize scales all of it
    // (RendererState::shadowAtlasWidth, shadowTileTexels()).
    inline constexpr uint32_t k_shadowTileSize{1024};
    inline constexpr uint32_t k_localShadowTileSize{512};
    inline constexpr uint32_t k_shadowAtlasWidth{k_shadowTileSize * 4};
    inline constexpr uint32_t k_shadowAtlasHeight{k_shadowTileSize * 2};

    // Environment atlas layout. Must match shaders/octahedral.glsl.
    inline constexpr uint32_t k_environmentTileSize{64};
    inline constexpr uint32_t k_environmentTiles{7};
} //namespace lockstep::render

#endif //LOCKSTEP_RENDER_FRAME_ATTACHMENTS_HPP
