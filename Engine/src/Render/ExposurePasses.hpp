#ifndef LOCKSTEP_RENDER_EXPOSURE_PASSES_HPP
#define LOCKSTEP_RENDER_EXPOSURE_PASSES_HPP

#include "Lockstep/Core/Result.hpp"

namespace vela::graphics
{
    class RenderGraph;
} //namespace vela::graphics

namespace lockstep::render
{
    struct RendererState;

    // Adds the auto exposure passes after the lit scene pass: the scene's log luminance on a
    // k_luminanceGridSize grid, its reduction to k_luminanceReducedSize, and the adapted exposure
    // (1x1, kept across frames and moved towards this frame's target). The exposure attachment's
    // red channel is the adapted exposure in stops; the last bloom pass turns it into the
    // multiplier tonemapping applies (see FrameAttachments.hpp). The passes always run, so the
    // adaptation state stays warm while auto exposure is switched off.
    // graph: the frame graph, already holding the scene passes.
    // state: the renderer state the passes read every frame. Must outlive the graph.
    // Returns: success, or an error if a shader is missing or the graph rejected a pass.
    [[nodiscard]] Status addExposurePasses(vela::graphics::RenderGraph& graph, RendererState& state);
} //namespace lockstep::render

#endif //LOCKSTEP_RENDER_EXPOSURE_PASSES_HPP
