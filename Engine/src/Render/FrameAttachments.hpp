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
    // shadowMap:     the 2x2 shadow atlas: sun cascades, the last tile possibly a spot light.
    // color:         HDR direct light, emissive, sky and transparent surfaces (lit scene pass).
    // ambient:       HDR environment / ambient light of opaque surfaces, darkened later by SSAO.
    // depth:         the scene depth buffer.
    // velocity:      per-pixel motion of the lit scene: xy screen motion since last frame in uv
    //                units (current minus previous), z the surface's view depth last frame (0 when
    //                unknown), for TAA.
    // reflection*:   the planar reflection view: colour (lit, ambient included) and the
    //                ambient, velocity and depth targets it needs to share the lit pass's
    //                pipelines; reduced resolution.
    // *Msaa:         multisampled colour, ambient, velocity and depth of the lit scene pass when MSAA is on,
    //                resolved into color, ambient and depth (depth takes sample zero).
    // occlusion:     raw SSAO, half resolution.
    // occlusionBlur: depth-aware blurred SSAO, half resolution; what composition reads.
    // bloomDown*/bloomUp*: the bloom mip chain; bloomUp1 is the result.
    // ldr:           the tonemapped image, linear, before anti-aliasing.
    // taaHistory:    the TAA result, kept across frames (persistent, double-buffered by Vela):
    //                rgb the accumulated image, a this pixel's view depth (0 marks invalid).
    // display:       the anti-aliased, dithered image that is presented; Engine overlays draw on it.
    inline constexpr std::string_view k_environmentAttachment{"environment"};
    inline constexpr std::string_view k_shadowMapAttachment{"shadowMap"};
    inline constexpr std::string_view k_sceneColorAttachment{"color"};
    inline constexpr std::string_view k_sceneAmbientAttachment{"ambient"};
    inline constexpr std::string_view k_sceneDepthAttachment{"depth"};
    inline constexpr std::string_view k_sceneVelocityAttachment{"velocity"};
    inline constexpr std::string_view k_sceneVelocityMsaaAttachment{"velocityMsaa"};
    inline constexpr std::string_view k_reflectionColorAttachment{"reflectionColor"};
    inline constexpr std::string_view k_reflectionAmbientAttachment{"reflectionAmbient"};
    inline constexpr std::string_view k_reflectionVelocityAttachment{"reflectionVelocity"};
    inline constexpr std::string_view k_reflectionDepthAttachment{"reflectionDepth"};
    inline constexpr std::string_view k_taaHistoryAttachment{"taaHistory"};
    inline constexpr std::string_view k_sceneColorMsaaAttachment{"colorMsaa"};
    inline constexpr std::string_view k_sceneAmbientMsaaAttachment{"ambientMsaa"};
    inline constexpr std::string_view k_sceneDepthMsaaAttachment{"depthMsaa"};
    inline constexpr std::string_view k_occlusionAttachment{"occlusion"};
    inline constexpr std::string_view k_occlusionBlurAttachment{"occlusionBlur"};
    inline constexpr std::string_view k_ldrAttachment{"ldr"};
    inline constexpr std::string_view k_displayAttachment{"display"};

    // Bloom chain: k_bloomLevels downsampled levels (each half the previous, starting at half
    // resolution) and k_bloomLevels - 1 upsampled ones.
    inline constexpr uint32_t k_bloomLevels{5};
    inline constexpr std::string_view k_bloomDownAttachments[k_bloomLevels]{"bloomDown1", "bloomDown2",
        "bloomDown3", "bloomDown4", "bloomDown5"};
    inline constexpr std::string_view k_bloomUpAttachments[k_bloomLevels - 1]{"bloomUp1", "bloomUp2",
        "bloomUp3", "bloomUp4"};

    inline constexpr vela::graphics::TextureFormat k_sceneColorFormat{vela::graphics::TextureFormat::RGBA16Float};
    inline constexpr vela::graphics::TextureFormat k_depthFormat{vela::graphics::TextureFormat::Depth32Float};
    inline constexpr vela::graphics::TextureFormat k_occlusionFormat{vela::graphics::TextureFormat::R8Unorm};
    inline constexpr vela::graphics::TextureFormat k_displayFormat{vela::graphics::TextureFormat::RGBA16Float};

    // Side length of one shadow atlas tile, in texels; the atlas is 2x2 tiles.
    inline constexpr uint32_t k_shadowTileSize{1024};
    inline constexpr uint32_t k_shadowAtlasSize{k_shadowTileSize * 2};

    // Environment atlas layout. Must match shaders/octahedral.glsl.
    inline constexpr uint32_t k_environmentTileSize{64};
    inline constexpr uint32_t k_environmentTiles{7};
} //namespace lockstep::render

#endif //LOCKSTEP_RENDER_FRAME_ATTACHMENTS_HPP
