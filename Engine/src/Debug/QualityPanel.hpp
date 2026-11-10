#ifndef LOCKSTEP_DEBUG_QUALITY_PANEL_HPP
#define LOCKSTEP_DEBUG_QUALITY_PANEL_HPP

#include "Render/RendererState.hpp"

namespace lockstep::debug
{
    // Draws the Graphics window's Quality section: GPU memory in use, the preset dropdown (applied
    // live; texture resolution needs a level reload), every GraphicsQuality field (changing one makes
    // the preset Custom) and a button saving them to the settings file. Must be called inside the
    // Graphics window.
    // renderer: the state of the renderer whose quality is edited.
    void drawQualitySection(render::RendererState& renderer);
} //namespace lockstep::debug

#endif //LOCKSTEP_DEBUG_QUALITY_PANEL_HPP
