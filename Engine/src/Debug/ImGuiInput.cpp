#include "ImGuiInput.hpp"

namespace lockstep::debug
{
    namespace
    {
        ImGuiKey offsetKey(ImGuiKey first, vela::core::Key key, vela::core::Key firstVelaKey)
        {
            return static_cast<ImGuiKey>(static_cast<int>(first) + static_cast<int>(key) - static_cast<int>(firstVelaKey));
        }

        ImGuiKey toImGuiKey(vela::core::Key key)
        {
            using vela::core::Key;

            if (key >= Key::A && key <= Key::Z)
            {
                return offsetKey(ImGuiKey_A, key, Key::A);
            }

            if (key >= Key::Num0 && key <= Key::Num9)
            {
                return offsetKey(ImGuiKey_0, key, Key::Num0);
            }

            if (key >= Key::F1 && key <= Key::F12)
            {
                return offsetKey(ImGuiKey_F1, key, Key::F1);
            }

            switch (key)
            {
                case Key::Escape:
                    return ImGuiKey_Escape;
                case Key::Enter:
                    return ImGuiKey_Enter;
                case Key::NumpadEnter:
                    return ImGuiKey_KeypadEnter;
                case Key::Tab:
                    return ImGuiKey_Tab;
                case Key::Backspace:
                    return ImGuiKey_Backspace;
                case Key::Space:
                    return ImGuiKey_Space;
                case Key::Insert:
                    return ImGuiKey_Insert;
                case Key::Delete:
                    return ImGuiKey_Delete;
                case Key::Left:
                    return ImGuiKey_LeftArrow;
                case Key::Right:
                    return ImGuiKey_RightArrow;
                case Key::Up:
                    return ImGuiKey_UpArrow;
                case Key::Down:
                    return ImGuiKey_DownArrow;
                case Key::PageUp:
                    return ImGuiKey_PageUp;
                case Key::PageDown:
                    return ImGuiKey_PageDown;
                case Key::Home:
                    return ImGuiKey_Home;
                case Key::End:
                    return ImGuiKey_End;
                case Key::LeftShift:
                    return ImGuiKey_LeftShift;
                case Key::LeftControl:
                    return ImGuiKey_LeftCtrl;
                case Key::LeftAlt:
                    return ImGuiKey_LeftAlt;
                case Key::LeftSuper:
                    return ImGuiKey_LeftSuper;
                case Key::RightShift:
                    return ImGuiKey_RightShift;
                case Key::RightControl:
                    return ImGuiKey_RightCtrl;
                case Key::RightAlt:
                    return ImGuiKey_RightAlt;
                case Key::RightSuper:
                    return ImGuiKey_RightSuper;
                case Key::Minus:
                    return ImGuiKey_Minus;
                case Key::Period:
                    return ImGuiKey_Period;
                case Key::Comma:
                    return ImGuiKey_Comma;
                default:
                    return ImGuiKey_None;
            }
        }

        int toImGuiMouseButton(vela::core::MouseButton button)
        {
            switch (button)
            {
                case vela::core::MouseButton::Left:
                    return ImGuiMouseButton_Left;
                case vela::core::MouseButton::Right:
                    return ImGuiMouseButton_Right;
                case vela::core::MouseButton::Middle:
                    return ImGuiMouseButton_Middle;
                default:
                    return -1;
            }
        }

        void forwardKeyEvent(ImGuiIO& io, const vela::core::Event& event)
        {
            io.AddKeyEvent(ImGuiMod_Shift, event.modifiers.shift);
            io.AddKeyEvent(ImGuiMod_Ctrl, event.modifiers.control);
            io.AddKeyEvent(ImGuiMod_Alt, event.modifiers.alt);
            io.AddKeyEvent(ImGuiMod_Super, event.modifiers.super);

            if (const ImGuiKey key = toImGuiKey(event.key); key != ImGuiKey_None)
            {
                io.AddKeyEvent(key, event.type == vela::core::EventType::KeyPressed);
            }
        }
    } //namespace

    void forwardEvent(ImGuiIO& io, const vela::core::Event& event)
    {
        switch (event.type)
        {
            case vela::core::EventType::MouseMoved:
                io.AddMousePosEvent(event.mouseX, event.mouseY);
                break;
            case vela::core::EventType::MouseButtonPressed:
            case vela::core::EventType::MouseButtonReleased:
                if (const int button = toImGuiMouseButton(event.button); button >= 0)
                {
                    io.AddMouseButtonEvent(button, event.type == vela::core::EventType::MouseButtonPressed);
                }
                break;
            case vela::core::EventType::MouseScrolled:
                io.AddMouseWheelEvent(event.scrollX, event.scrollY);
                break;
            case vela::core::EventType::KeyPressed:
            case vela::core::EventType::KeyReleased:
                forwardKeyEvent(io, event);
                break;
            case vela::core::EventType::TextEntered:
                io.AddInputCharacter(event.codepoint);
                break;
            case vela::core::EventType::FocusGained:
                io.AddFocusEvent(true);
                break;
            case vela::core::EventType::FocusLost:
                io.AddFocusEvent(false);
                break;
            default:
                break;
        }
    }
} //namespace lockstep::debug
