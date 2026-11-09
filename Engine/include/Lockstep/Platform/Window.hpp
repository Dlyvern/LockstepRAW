#ifndef LOCKSTEP_PLATFORM_WINDOW_HPP
#define LOCKSTEP_PLATFORM_WINDOW_HPP

#include "Lockstep/Core/Result.hpp"
#include "Lockstep/Platform/Input.hpp"

#include <memory>
#include <string>

namespace lockstep::platform
{
    struct WindowState;

    // Settings for the OS window.
    struct WindowDescription
    {
        std::string title{"Lockstep"};
        int width{1280};
        int height{720};
    };

    // The OS window and its input. pollEvents() once per frame turns the window's events into
    // an Input snapshot. The Renderer draws into this window, so the window must outlive it.
    class Window
    {
    public:
        // Opens a window.
        // description: title and size.
        // Returns:     the window, or an error if the OS window could not be created.
        [[nodiscard]] static Result<Window> create(const WindowDescription& description);

        ~Window();

        Window(Window&& other) noexcept;
        Window& operator=(Window&& other) noexcept;

        Window(const Window&) = delete;
        Window& operator=(const Window&) = delete;

        // Returns: true until the window is closed by the user or by close().
        [[nodiscard]] bool isOpen() const;

        // Closes the window. isOpen() returns false afterwards.
        void close();

        // Processes pending OS events and rebuilds input() from them. Call once per frame.
        void pollEvents();

        // Returns: input of the last pollEvents(): held state, changes since the previous
        //          pollEvents() and mouse movement. Not filtered for the dev tools.
        [[nodiscard]] const Input& input() const;

        // Captures or releases the cursor. A captured cursor is hidden and unbounded, for mouse
        // look; read its movement from Input::mouseDelta().
        // captured: true to capture.
        void setCursorCaptured(bool captured);

        // Returns: true while the cursor is captured.
        [[nodiscard]] bool isCursorCaptured() const;

    private:
        friend WindowState& windowState(Window& window);

        explicit Window(std::unique_ptr<WindowState> state);

        std::unique_ptr<WindowState> m_state;
    };
} //namespace lockstep::platform

#endif //LOCKSTEP_PLATFORM_WINDOW_HPP
