#ifndef LOCKSTEP_RENDER_INDIRECT_LIGHTING_HPP
#define LOCKSTEP_RENDER_INDIRECT_LIGHTING_HPP

#include "Lockstep/Core/Result.hpp"

namespace vela::graphics
{
    class RenderGraph;
} //namespace vela::graphics

namespace lockstep::render
{
    struct RendererState;

    // Adds the screen-space indirect lighting passes right after the lit scene pass, before the
    // volumetric fog:
    // - occlusion / occlusionBlur: GTAO (diffuse and specular visibility) and sun contact shadows
    //   at half resolution from depth and the lit pass's G-buffer, then a depth-aware blur;
    // - screen-space reflections (see addScreenSpaceReflectionPasses());
    // - the indirect composite, which rebuilds the ambient target from ambientLit (multi-bounce
    //   occlusion, reflections over the environment specular, specular occlusion) and darkens the
    //   colour target by the contact shadows.
    // Every pass but the composite skips its draw when its feature is off; the composite always
    // runs, since it produces the ambient target the rest of the frame reads.
    // graph: the frame graph, already holding the lit scene pass.
    // state: the renderer state the passes read every frame. Must outlive the graph.
    // Returns: success, or an error if a shader is missing or the graph rejected a pass.
    [[nodiscard]] Status addIndirectLightingPasses(vela::graphics::RenderGraph& graph, RendererState& state);
} //namespace lockstep::render

#endif //LOCKSTEP_RENDER_INDIRECT_LIGHTING_HPP
