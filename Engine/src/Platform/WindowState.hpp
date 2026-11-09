#ifndef LOCKSTEP_PLATFORM_WINDOW_STATE_HPP
#define LOCKSTEP_PLATFORM_WINDOW_STATE_HPP

#include "Lockstep/Core/Math.hpp"
#include "Lockstep/Platform/Input.hpp"
#include "Lockstep/Platform/Window.hpp"

#include <Vela/Core/Window.hpp>

#include <optional>

namespace lockstep::platform
{
    // Everything a Window owns, kept behind a pointer so the public header stays free of Vela and
    // the Vela window keeps a stable address (the Vela context created on it refers to it).
    struct WindowState
    {
        // Takes ownership of an open Vela window.
        // window: the window.
        explicit WindowState(vela::core::Window&& window);

        vela::core::Window window;
        Input input;
        std::optional<math::Vec2> lastCursorPosition;
        bool isCursorCaptured{false};
    };

    // Engine-internal access to a Window's state, for Engine modules that need the Vela window
    // (Render creates its GPU context on it, Debug feeds its raw events to ImGui). Game cannot
    // use it: WindowState is private.
    // window:  the window.
    // Returns: its state, which lives as long as the window.
    [[nodiscard]] WindowState& windowState(Window& window);
} //namespace lockstep::platform

#endif //LOCKSTEP_PLATFORM_WINDOW_STATE_HPP
