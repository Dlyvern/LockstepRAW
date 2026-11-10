#ifndef LOCKSTEP_DEBUG_DETAIL_LENS_PANEL_HPP
#define LOCKSTEP_DEBUG_DETAIL_LENS_PANEL_HPP

#include "Lockstep/Render/RenderSettings.hpp"

namespace lockstep::debug
{
    // Draws the Graphics window's surface detail section (parallax occlusion mapping, micro-detail)
    // and lens section (motion blur, depth of field, chromatic aberration, lens dirt), edited in
    // place. Must be called inside the Graphics window.
    // settings: the renderer settings to tune.
    void drawDetailLensSections(render::RenderSettings& settings);
} //namespace lockstep::debug

#endif //LOCKSTEP_DEBUG_DETAIL_LENS_PANEL_HPP
