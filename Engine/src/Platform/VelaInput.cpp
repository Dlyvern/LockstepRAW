#include "VelaInput.hpp"

namespace lockstep::platform
{
    namespace
    {
        Key offsetKey(Key first, vela::core::Key key, vela::core::Key firstVelaKey)
        {
            return static_cast<Key>(static_cast<int>(first) + static_cast<int>(key) - static_cast<int>(firstVelaKey));
        }
    } //namespace

    std::optional<Key> fromVelaKey(vela::core::Key key)
    {
        using VelaKey = vela::core::Key;

        if (key >= VelaKey::A && key <= VelaKey::Z)
        {
            return offsetKey(Key::A, key, VelaKey::A);
        }

        if (key >= VelaKey::Num0 && key <= VelaKey::Num9)
        {
            return offsetKey(Key::Num0, key, VelaKey::Num0);
        }

        if (key >= VelaKey::Numpad0 && key <= VelaKey::Numpad9)
        {
            return offsetKey(Key::Num0, key, VelaKey::Numpad0);
        }

        if (key >= VelaKey::F1 && key <= VelaKey::F12)
        {
            return offsetKey(Key::F1, key, VelaKey::F1);
        }

        switch (key)
        {
            case VelaKey::Escape:
            {
                return Key::Escape;
            }
            case VelaKey::Enter:
            {
                return Key::Enter;
            }
            case VelaKey::Tab:
            {
                return Key::Tab;
            }
            case VelaKey::Backspace:
            {
                return Key::Backspace;
            }
            case VelaKey::Space:
            {
                return Key::Space;
            }
            case VelaKey::Left:
            {
                return Key::Left;
            }
            case VelaKey::Right:
            {
                return Key::Right;
            }
            case VelaKey::Up:
            {
                return Key::Up;
            }
            case VelaKey::Down:
            {
                return Key::Down;
            }
            case VelaKey::LeftShift:
            {
                return Key::LeftShift;
            }
            case VelaKey::LeftControl:
            {
                return Key::LeftControl;
            }
            case VelaKey::LeftAlt:
            {
                return Key::LeftAlt;
            }
            case VelaKey::RightShift:
            {
                return Key::RightShift;
            }
            case VelaKey::RightControl:
            {
                return Key::RightControl;
            }
            case VelaKey::RightAlt:
            {
                return Key::RightAlt;
            }
            case VelaKey::Delete:
            {
                return Key::Delete;
            }
            case VelaKey::Insert:
            {
                return Key::Insert;
            }
            case VelaKey::Home:
            {
                return Key::Home;
            }
            case VelaKey::End:
            {
                return Key::End;
            }
            case VelaKey::PageUp:
            {
                return Key::PageUp;
            }
            case VelaKey::PageDown:
            {
                return Key::PageDown;
            }
            case VelaKey::CapsLock:
            {
                return Key::CapsLock;
            }
            case VelaKey::Minus:
            {
                return Key::Minus;
            }
            case VelaKey::Equal:
            {
                return Key::Equal;
            }
            case VelaKey::LeftBracket:
            {
                return Key::LeftBracket;
            }
            case VelaKey::RightBracket:
            {
                return Key::RightBracket;
            }
            case VelaKey::Backslash:
            {
                return Key::Backslash;
            }
            case VelaKey::Semicolon:
            {
                return Key::Semicolon;
            }
            case VelaKey::Apostrophe:
            {
                return Key::Apostrophe;
            }
            case VelaKey::Comma:
            {
                return Key::Comma;
            }
            case VelaKey::Period:
            {
                return Key::Period;
            }
            case VelaKey::Slash:
            {
                return Key::Slash;
            }
            case VelaKey::GraveAccent:
            {
                return Key::GraveAccent;
            }
            case VelaKey::NumpadEnter:
            {
                return Key::Enter;
            }
            case VelaKey::NumpadDecimal:
            {
                return Key::Period;
            }
            default:
            {
                return std::nullopt;
            }
        }
    }

    std::optional<MouseButton> fromVelaMouseButton(vela::core::MouseButton button)
    {
        switch (button)
        {
            case vela::core::MouseButton::Left:
            {
                return MouseButton::Left;
            }
            case vela::core::MouseButton::Right:
            {
                return MouseButton::Right;
            }
            case vela::core::MouseButton::Middle:
            {
                return MouseButton::Middle;
            }
            case vela::core::MouseButton::Extra1:
            {
                return MouseButton::Back;
            }
            case vela::core::MouseButton::Extra2:
            {
                return MouseButton::Forward;
            }
            default:
            {
                return std::nullopt;
            }
        }
    }
} //namespace lockstep::platform
