#ifndef LOCKSTEP_DEBUG_ENGINE_PANEL_HPP
#define LOCKSTEP_DEBUG_ENGINE_PANEL_HPP

#include "Render/RendererState.hpp"

namespace lockstep::physics
{
    class PhysicsWorld;
} //namespace lockstep::physics

namespace lockstep::debug
{
    // Draws the "Engine" ImGui window: frame rate and times, CPU and GPU frame times, VRAM use,
    // GPU allocation count, Vulkan validation message count and the GPU name. Must be called
    // inside an ImGui frame.
    // renderer: the state of the renderer to report on.
    void drawEnginePanel(const render::RendererState& renderer);

    // Appends a Physics section to the "Engine" ImGui window: body and controller counts, last
    // step time and substeps, PhysX error and warning counts. Must be called inside an ImGui
    // frame, after drawEnginePanel().
    // physics: the physics world to report on.
    void drawPhysicsSection(const physics::PhysicsWorld& physics);
} //namespace lockstep::debug

#endif //LOCKSTEP_DEBUG_ENGINE_PANEL_HPP
