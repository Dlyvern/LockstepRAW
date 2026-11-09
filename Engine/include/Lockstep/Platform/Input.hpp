#ifndef LOCKSTEP_PLATFORM_INPUT_HPP
#define LOCKSTEP_PLATFORM_INPUT_HPP

#include "Lockstep/Core/Math.hpp"

#include <bitset>
#include <cstddef>
#include <cstdint>

namespace lockstep::platform
{
    // Keyboard keys the Engine reports. Keys outside this set are ignored.
    enum class Key : uint8_t
    {
        A, B, C, D, E, F, G, H, I, J, K, L, M, N, O, P, Q, R, S, T, U, V, W, X, Y, Z,
        Num0, Num1, Num2, Num3, Num4, Num5, Num6, Num7, Num8, Num9,
        F1, F2, F3, F4, F5, F6, F7, F8, F9, F10, F11, F12,
        Escape,
        Enter,
        Tab,
        Backspace,
        Space,
        Left,
        Right,
        Up,
        Down,
        LeftShift,
        LeftControl,
        LeftAlt,
        RightShift,
        RightControl,
        RightAlt,
        Count
    };

    // Mouse buttons the Engine reports.
    enum class MouseButton : uint8_t
    {
        Left,
        Right,
        Middle,
        Count
    };

    struct InputAccess;

    // A snapshot of keyboard and mouse state: what is held, what changed and how far the mouse
    // moved. A plain value, cheap to copy. The run loop hands Game one per frame (changes since
    // the previous frame) and one per fixed tick (changes since the previous tick, so a press is
    // seen by exactly one tick even when a frame runs zero or several ticks).
    class Input
    {
    public:
        // key:     the key to query.
        // Returns: true if `key` is held down.
        [[nodiscard]] bool isKeyDown(Key key) const;

        // key:     the key to query.
        // Returns: true if `key` went down during the period this snapshot covers. Key repeat
        //          does not count as a new press.
        [[nodiscard]] bool wasKeyPressed(Key key) const;

        // key:     the key to query.
        // Returns: true if `key` went up during the period this snapshot covers.
        [[nodiscard]] bool wasKeyReleased(Key key) const;

        // button:  the mouse button to query.
        // Returns: true if `button` is held down.
        [[nodiscard]] bool isMouseButtonDown(MouseButton button) const;

        // button:  the mouse button to query.
        // Returns: true if `button` went down during the period this snapshot covers.
        [[nodiscard]] bool wasMouseButtonPressed(MouseButton button) const;

        // button:  the mouse button to query.
        // Returns: true if `button` went up during the period this snapshot covers.
        [[nodiscard]] bool wasMouseButtonReleased(MouseButton button) const;

        // Returns: cursor movement in window pixels (x right, y down) during the period this
        //          snapshot covers. With a captured cursor the movement is unbounded, which is
        //          what mouse look needs.
        [[nodiscard]] math::Vec2 mouseDelta() const;

        // Returns: mouse wheel movement during the period this snapshot covers (y is the usual
        //          vertical wheel).
        [[nodiscard]] math::Vec2 scrollDelta() const;

    private:
        friend struct InputAccess;

        static constexpr std::size_t k_keyCount{static_cast<std::size_t>(Key::Count)};
        static constexpr std::size_t k_mouseButtonCount{static_cast<std::size_t>(MouseButton::Count)};

        std::bitset<k_keyCount> m_keysDown;
        std::bitset<k_keyCount> m_keysPressed;
        std::bitset<k_keyCount> m_keysReleased;
        std::bitset<k_mouseButtonCount> m_buttonsDown;
        std::bitset<k_mouseButtonCount> m_buttonsPressed;
        std::bitset<k_mouseButtonCount> m_buttonsReleased;
        math::Vec2 m_mouseDelta{0.0f, 0.0f};
        math::Vec2 m_scrollDelta{0.0f, 0.0f};
    };
} //namespace lockstep::platform

#endif //LOCKSTEP_PLATFORM_INPUT_HPP
