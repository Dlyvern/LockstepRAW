#ifndef LOCKSTEP_RENDER_VOLUMETRIC_FOG_HPP
#define LOCKSTEP_RENDER_VOLUMETRIC_FOG_HPP

#include "Lockstep/Core/Result.hpp"

namespace vela::graphics
{
    class RenderGraph;
} //namespace vela::graphics

namespace lockstep::render
{
    struct RendererState;

    // Adds the volumetric fog passes after the lit scene pass (see VolumetricFogSettings):
    // the jittered raymarch into fogScatter at reduced resolution (sun cascades and local light
    // tiles of the shadow atlas shape the light), the temporal blend into fogResolved, and the
    // composite that upsamples it by depth into the colour target (in-scattered light, over the
    // transmitted scene) and the ambient target (transmitted), so later passes see fogged HDR.
    // When the fog is off the passes skip their draws.
    // graph: the frame graph, already holding the lit scene pass.
    // state: the renderer state the passes read every frame. Must outlive the graph.
    // Returns: success, or an error if a shader is missing or the graph rejected a pass.
    [[nodiscard]] Status addVolumetricFogPasses(vela::graphics::RenderGraph& graph, RendererState& state);

    // Clamps the fog resolution scale setting and redescribes the fog passes when it changed. On
    // failure the previous scale is restored.
    // state:   the renderer state.
    // Returns: success, or an error if the graph rejected the new description.
    [[nodiscard]] Status updateVolumetricFogScale(RendererState& state);
} //namespace lockstep::render

#endif //LOCKSTEP_RENDER_VOLUMETRIC_FOG_HPP
