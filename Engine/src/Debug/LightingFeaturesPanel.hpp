#ifndef LOCKSTEP_DEBUG_LIGHTING_FEATURES_PANEL_HPP
#define LOCKSTEP_DEBUG_LIGHTING_FEATURES_PANEL_HPP

namespace lockstep::render
{
    struct RendererState;
} //namespace lockstep::render

namespace lockstep::debug
{
    // Draws the Graphics window's light shape, soft shadow, baked bounce light and shadow caster
    // sections, editing the requested settings in place and showing the loaded irradiance volume.
    // Must be called inside the Graphics window.
    // renderer: the renderer state.
    void drawLightingFeatureSections(render::RendererState& renderer);
} //namespace lockstep::debug

#endif //LOCKSTEP_DEBUG_LIGHTING_FEATURES_PANEL_HPP
