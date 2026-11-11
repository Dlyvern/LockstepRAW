#ifndef LOCKSTEP_RENDER_SURFACE_EFFECTS_PACKING_HPP
#define LOCKSTEP_RENDER_SURFACE_EFFECTS_PACKING_HPP

namespace lockstep::render
{
    struct RendererState;

    // Writes the surface effect fields of the frame's lighting block (surfaceEffects and the drip
    // puddles nearest the camera, from RendererState::dripPuddles and
    // RenderSettings::surfaceEffects). Call after prepareLighting().
    // state: the renderer state.
    void packSurfaceEffects(RendererState& state);
} //namespace lockstep::render

#endif //LOCKSTEP_RENDER_SURFACE_EFFECTS_PACKING_HPP
