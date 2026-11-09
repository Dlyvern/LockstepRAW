#ifndef LOCKSTEP_DEBUG_ENGINE_PANEL_HPP
#define LOCKSTEP_DEBUG_ENGINE_PANEL_HPP

#include "FrameTimeHistory.hpp"

#include "Render/RendererState.hpp"

namespace lockstep::physics
{
    class PhysicsWorld;
} //namespace lockstep::physics

namespace lockstep::debug
{
    // Draws the "Engine" ImGui window: the GPU name, the present mode in use (and the one asked
    // for), frame rate, min/avg/max of frame time, CPU work and Vela frame time (which includes
    // the vsync wait), the last GPU frame time, VRAM use, GPU allocation count and Vulkan
    // validation message count. Must be called inside an ImGui frame.
    // renderer:   the state of the renderer to report on.
    // frameTimes: the recent per-frame durations recorded by the run loop.
    void drawEnginePanel(const render::RendererState& renderer, const FrameTimeHistories& frameTimes);

    // Appends a Physics section to the "Engine" ImGui window: body and controller counts, last
    // step time and substeps, PhysX error and warning counts. Must be called inside an ImGui
    // frame, after drawEnginePanel().
    // physics: the physics world to report on.
    void drawPhysicsSection(const physics::PhysicsWorld& physics);
} //namespace lockstep::debug

#endif //LOCKSTEP_DEBUG_ENGINE_PANEL_HPP
