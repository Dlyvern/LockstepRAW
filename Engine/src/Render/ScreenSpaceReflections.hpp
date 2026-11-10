#ifndef LOCKSTEP_RENDER_SCREEN_SPACE_REFLECTIONS_HPP
#define LOCKSTEP_RENDER_SCREEN_SPACE_REFLECTIONS_HPP

#include "Lockstep/Core/Result.hpp"

namespace vela::graphics
{
    class RenderGraph;
} //namespace vela::graphics

namespace lockstep::render
{
    struct RendererState;

    // Adds the screen-space reflection passes (see ScreenSpaceReflectionSettings), all at half
    // resolution: the ray march into ssrHit (depth and the lit pass's G-buffer), the resolve that
    // fetches last frame's ssrSource at the hits and accumulates into ssrResolved, and the capture
    // of the finished lit scene into ssrSource for the next frame. The indirect composite reads
    // ssrResolved. When reflections are off the passes skip their draws (ssrResolved is then 0).
    // graph: the frame graph, already holding the lit scene pass.
    // state: the renderer state the passes read every frame. Must outlive the graph.
    // Returns: success, or an error if a shader is missing or the graph rejected a pass.
    [[nodiscard]] Status addScreenSpaceReflectionPasses(vela::graphics::RenderGraph& graph, RendererState& state);

    // Returns: whether screen-space reflections run this frame.
    // state:   the renderer state.
    [[nodiscard]] bool screenSpaceReflectionsActive(const RendererState& state);

    // The (P[0][0], P[1][1], P[2][2], P[3][2]) camera projection terms the screen-space shaders
    // reconstruct view positions from (shaders/view_reconstruct.glsl).
    struct ProjectionTerms
    {
        float values[4];
    };

    // Returns: this frame's projection terms.
    // state:   the renderer state with this frame's projection.
    [[nodiscard]] ProjectionTerms projectionTerms(const RendererState& state);

    // Returns: a per-frame noise offset for jittered screen-space passes: changes every frame
    //          while TAA accumulates, 0 otherwise (stable noise without temporal filtering).
    // state:   the renderer state.
    [[nodiscard]] float temporalNoiseOffset(const RendererState& state);
} //namespace lockstep::render

#endif //LOCKSTEP_RENDER_SCREEN_SPACE_REFLECTIONS_HPP
