#include "Lockstep/Platform/Window.hpp"

#include "Lockstep/Core/Log.hpp"

#include "InputAccess.hpp"
#include "VelaInput.hpp"
#include "WindowState.hpp"

#include "Core/VelaError.hpp"

#include <Vela/Core/Input.hpp>

#include <utility>

namespace lockstep::platform
{
    namespace
    {
        void applyKeyEvent(Input& input, const vela::core::Event& event)
        {
            if (const std::optional<Key> key = fromVelaKey(event.key))
            {
                InputAccess::setKey(input, *key, event.type == vela::core::EventType::KeyPressed);
            }
        }

        void applyMouseButtonEvent(Input& input, const vela::core::Event& event)
        {
            if (const std::optional<MouseButton> button = fromVelaMouseButton(event.button))
            {
                InputAccess::setMouseButton(input, *button, event.type == vela::core::EventType::MouseButtonPressed);
            }
        }

        void applyMouseMoveEvent(WindowState& state, const vela::core::Event& event)
        {
            const math::Vec2 position{event.mouseX, event.mouseY};

            if (state.lastCursorPosition)
            {
                InputAccess::addMouseDelta(state.input, position - *state.lastCursorPosition);
            }

            state.lastCursorPosition = position;
        }

        void applyEvent(WindowState& state, const vela::core::Event& event)
        {
            switch (event.type)
            {
                case vela::core::EventType::KeyPressed:
                case vela::core::EventType::KeyReleased:
                {
                    applyKeyEvent(state.input, event);
                    break;
                }
                case vela::core::EventType::MouseButtonPressed:
                case vela::core::EventType::MouseButtonReleased:
                {
                    applyMouseButtonEvent(state.input, event);
                    break;
                }
                case vela::core::EventType::MouseMoved:
                {
                    applyMouseMoveEvent(state, event);
                    break;
                }
                case vela::core::EventType::MouseScrolled:
                {
                    InputAccess::addScrollDelta(state.input, {event.scrollX, event.scrollY});
                    break;
                }
                case vela::core::EventType::FocusLost:
                {
                    InputAccess::releaseAll(state.input);
                    state.lastCursorPosition.reset();
                    break;
                }
                default:
                {
                    break;
                }
            }
        }
    } //namespace

    WindowState::WindowState(vela::core::Window&& window)
        : window(std::move(window))
    {
    }

    WindowState& windowState(Window& window)
    {
        return *window.m_state;
    }

    Result<Window> Window::create(const WindowDescription& description)
    {
        auto window = vela::core::Window::create({
            .title = description.title,
            .width = description.width,
            .height = description.height});

        if (!window)
        {
            return toError(window.error());
        }

        return Window(std::make_unique<WindowState>(std::move(window).value()));
    }

    Window::Window(std::unique_ptr<WindowState> state)
        : m_state(std::move(state))
    {
    }

    Window::~Window() = default;

    Window::Window(Window&& other) noexcept = default;

    Window& Window::operator=(Window&& other) noexcept = default;

    bool Window::isOpen() const
    {
        return m_state->window.isOpen();
    }

    void Window::close()
    {
        m_state->window.close();
    }

    void Window::pollEvents()
    {
        InputAccess::clearTransient(m_state->input);
        m_state->window.pollEvents();

        for (const vela::core::Event& event : m_state->window.events())
        {
            applyEvent(*m_state, event);
        }
    }

    const Input& Window::input() const
    {
        return m_state->input;
    }

    void Window::setCursorCaptured(bool captured)
    {
        if (m_state->isCursorCaptured == captured)
        {
            return;
        }

        m_state->window.setCursorMode(captured ? vela::core::CursorMode::Disabled : vela::core::CursorMode::Normal);
        m_state->isCursorCaptured = captured;

        if (m_state->hasRawMouseMotion)
        {
            if (auto raw = m_state->window.setRawMouseMotion(captured); !raw)
            {
                m_state->hasRawMouseMotion = false;
                log(LogLevel::Info, "Platform", "raw mouse motion is not available: " + raw.error().message);
            }
        }
        m_state->lastCursorPosition.reset();
    }

    bool Window::isCursorCaptured() const
    {
        return m_state->isCursorCaptured;
    }
} //namespace lockstep::platform
