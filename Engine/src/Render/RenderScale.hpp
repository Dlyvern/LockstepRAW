#ifndef LOCKSTEP_RENDER_RENDER_SCALE_HPP
#define LOCKSTEP_RENDER_RENDER_SCALE_HPP

#include "Lockstep/Core/Result.hpp"

#include <Vela/Graphics/Pass.hpp>

#include <memory>
#include <string_view>

namespace vela::graphics
{
    class RenderGraph;
} //namespace vela::graphics

namespace lockstep::render
{
    struct RendererState;

    // Adds a frame pass whose window-sized outputs follow RendererState::renderScale: every colour
    // and depth output without a fixed size, except the display attachment, has its scale multiplied
    // by it whenever the pass is described. Vela has no graph-wide resolution scale, so every scene
    // pass goes through this; the output pass upscales into the display.
    // graph:   the frame graph.
    // state:   the renderer state; must outlive the graph. The name is remembered for
    //          updateRenderScale().
    // name:    unique pass name.
    // pass:    the pass. Must not be null.
    // Returns: success, or an error if the graph rejected the pass.
    [[nodiscard]] Status addScaledPass(vela::graphics::RenderGraph& graph, RendererState& state, std::string_view name,
        std::unique_ptr<vela::graphics::Pass> pass);

    // Takes the render scale from RendererState::quality and redescribes every scaled pass when it
    // changed. On failure the previous scale is restored.
    // state:   the renderer state.
    // Returns: success, or an error if the graph rejected a new description.
    [[nodiscard]] Status updateRenderScale(RendererState& state);

    // Takes the shadow atlas width from RendererState::quality and redescribes the shadow pass when
    // it changed. On failure the previous size is restored.
    // state:   the renderer state.
    // Returns: success, or an error if the graph rejected the new description.
    [[nodiscard]] Status updateShadowAtlas(RendererState& state);
} //namespace lockstep::render

#endif //LOCKSTEP_RENDER_RENDER_SCALE_HPP
