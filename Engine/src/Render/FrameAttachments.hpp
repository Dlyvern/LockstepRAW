#ifndef LOCKSTEP_RENDER_FRAME_ATTACHMENTS_HPP
#define LOCKSTEP_RENDER_FRAME_ATTACHMENTS_HPP

#include <Vela/Graphics/RenderTypes.hpp>

#include <string_view>

namespace lockstep::render
{
    // Names and formats of the render graph attachments the Renderer's frame passes share.
    // shadowMap: depth from the shadow-casting light, written by the shadow pass.
    // color:     the HDR scene, written by the lit scene pass, read by tonemapping.
    // depth:     the scene depth buffer.
    // display:   the tonemapped image that is presented; Engine overlays draw on top of it.
    inline constexpr std::string_view k_shadowMapAttachment{"shadowMap"};
    inline constexpr std::string_view k_sceneColorAttachment{"color"};
    inline constexpr std::string_view k_sceneDepthAttachment{"depth"};
    inline constexpr std::string_view k_displayAttachment{"display"};

    inline constexpr vela::graphics::TextureFormat k_sceneColorFormat{vela::graphics::TextureFormat::RGBA16Float};
    inline constexpr vela::graphics::TextureFormat k_depthFormat{vela::graphics::TextureFormat::Depth32Float};
    inline constexpr vela::graphics::TextureFormat k_displayFormat{vela::graphics::TextureFormat::RGBA16Float};
} //namespace lockstep::render

#endif //LOCKSTEP_RENDER_FRAME_ATTACHMENTS_HPP
