#ifndef LOCKSTEP_DEBUG_IMGUI_INPUT_HPP
#define LOCKSTEP_DEBUG_IMGUI_INPUT_HPP

#include <Vela/Core/Input.hpp>

#include <imgui.h>

namespace lockstep::debug
{
    // Queues one window event as ImGui input. Events ImGui has no use for are ignored.
    // io:    the IO of the current ImGui context.
    // event: a window event from the last poll.
    void forwardEvent(ImGuiIO& io, const vela::core::Event& event);
} //namespace lockstep::debug

#endif //LOCKSTEP_DEBUG_IMGUI_INPUT_HPP
