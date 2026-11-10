#ifndef LOCKSTEP_DEBUG_INDIRECT_LIGHTING_PANEL_HPP
#define LOCKSTEP_DEBUG_INDIRECT_LIGHTING_PANEL_HPP

#include "Lockstep/Render/RenderSettings.hpp"

namespace lockstep::debug
{
    // Draws the Graphics window's screen-space lighting sections: ambient occlusion (GTAO),
    // contact shadows and screen-space reflections, edited in place. Must be called inside the
    // Graphics window.
    // settings: the renderer settings to tune.
    void drawIndirectLightingSections(render::RenderSettings& settings);
} //namespace lockstep::debug

#endif //LOCKSTEP_DEBUG_INDIRECT_LIGHTING_PANEL_HPP
