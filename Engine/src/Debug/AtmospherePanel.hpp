#ifndef LOCKSTEP_DEBUG_ATMOSPHERE_PANEL_HPP
#define LOCKSTEP_DEBUG_ATMOSPHERE_PANEL_HPP

namespace lockstep::render
{
    struct RenderSettings;
} //namespace lockstep::render

namespace lockstep::debug
{
    // Draws the Graphics window's cloud, wind, lightning and lens rain sections, editing the
    // requested settings in place. Values Game animates every frame (wind speed, cloud drift, the
    // flash) are shown but overwritten by Game on the next frame. Must be called inside the
    // Graphics window.
    // settings: the requested render settings.
    void drawAtmosphereSections(render::RenderSettings& settings);
} //namespace lockstep::debug

#endif //LOCKSTEP_DEBUG_ATMOSPHERE_PANEL_HPP
