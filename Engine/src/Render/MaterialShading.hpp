#ifndef LOCKSTEP_RENDER_MATERIAL_SHADING_HPP
#define LOCKSTEP_RENDER_MATERIAL_SHADING_HPP

namespace lockstep::render
{
    struct RendererState;

    // Fills every queued draw's DrawItem::shading from its material's shading model, its character
    // surface and the frame's (quality-limited) MaterialShadingSettings. Call once per render()
    // after the settings were limited, before any pass records.
    // state: the renderer state.
    void resolveMaterialShading(RendererState& state);
} //namespace lockstep::render

#endif //LOCKSTEP_RENDER_MATERIAL_SHADING_HPP
