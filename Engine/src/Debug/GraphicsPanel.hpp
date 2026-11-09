#ifndef LOCKSTEP_DEBUG_GRAPHICS_PANEL_HPP
#define LOCKSTEP_DEBUG_GRAPHICS_PANEL_HPP

#include "Render/RendererState.hpp"

namespace lockstep::debug
{
    // What the Graphics panel remembers between frames.
    // smoothedGpuMs: GPU frame time, exponentially smoothed so on/off comparisons are readable.
    struct GraphicsPanelState
    {
        float smoothedGpuMs{0.0f};
    };

    // Draws the "Graphics" ImGui window: smoothed GPU frame time, the frame's draw, light and
    // shadow counts, and every renderer feature switch and tunable (lighting, sun and sky, IBL,
    // shadows, SSAO, bloom, FXAA, culling, exposure, fog), edited in place. Must be called inside
    // an ImGui frame.
    // panel:    the panel's state across frames.
    // renderer: the state of the renderer to tune.
    void drawGraphicsPanel(GraphicsPanelState& panel, render::RendererState& renderer);
} //namespace lockstep::debug

#endif //LOCKSTEP_DEBUG_GRAPHICS_PANEL_HPP
