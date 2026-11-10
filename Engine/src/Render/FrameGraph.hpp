#ifndef LOCKSTEP_RENDER_FRAME_GRAPH_HPP
#define LOCKSTEP_RENDER_FRAME_GRAPH_HPP

#include "Lockstep/Core/Result.hpp"

namespace lockstep::render
{
    struct RendererState;

    // Builds the Engine frame graph into RendererState::graph: environment, shadow, planar
    // reflection and lit scene passes, the indirect lighting passes (ambient occlusion, contact
    // shadows, screen-space reflections), the volumetric fog passes, then the post passes, presenting
    // the display attachment.
    // state:   the renderer state the passes read every frame. Must outlive the graph.
    // Returns: success, or an error if a pass could not be created or added.
    [[nodiscard]] Status buildFrameGraph(RendererState& state);

    // Adds the screen overlay pass (text, rectangles, images) as the first Engine pass and stores it
    // in RendererState::overlayPass.
    // state:   the renderer state, with its graph built.
    // Returns: success, or an error if the pass could not be created or added.
    [[nodiscard]] Status addOverlayPass(RendererState& state);

    // Matches the scene pass's sample count to the anti-aliasing mode (clamped to what the device
    // supports), redescribing the pass when it changes. On failure anti-aliasing is switched off (in the quality too).
    // state:   the renderer state.
    // Returns: success, or an error if the graph rejected the new description.
    [[nodiscard]] Status updateSceneSamples(RendererState& state);

    // Clamps the reflection resolution scale setting and redescribes the reflection pass when it
    // changed. On failure the previous scale is restored.
    // state:   the renderer state.
    // Returns: success, or an error if the graph rejected the new description.
    [[nodiscard]] Status updateReflectionScale(RendererState& state);
} //namespace lockstep::render

#endif //LOCKSTEP_RENDER_FRAME_GRAPH_HPP
