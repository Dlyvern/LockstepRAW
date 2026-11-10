#ifndef LOCKSTEP_DEBUG_GRAPHICS_LOOK_PANEL_HPP
#define LOCKSTEP_DEBUG_GRAPHICS_LOOK_PANEL_HPP

#include "Lockstep/Render/RenderSettings.hpp"

#include <cstddef>

namespace lockstep::debug
{
    // Draws the Graphics window's look sections: interior ambient zones, volumetric fog, surface
    // wetness, auto exposure, colour grading, vignette and film grain, edited in place. Must be
    // called inside the Graphics window.
    // settings:  the renderer settings to tune.
    // zoneCount: interior ambient zones the renderer holds (shown for reference).
    void drawLookSections(render::RenderSettings& settings, std::size_t zoneCount);
} //namespace lockstep::debug

#endif //LOCKSTEP_DEBUG_GRAPHICS_LOOK_PANEL_HPP
