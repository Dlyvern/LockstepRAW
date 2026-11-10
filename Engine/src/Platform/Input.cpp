#include "Lockstep/Platform/Input.hpp"

#include "InputAccess.hpp"

#include <algorithm>
#include <array>
#include <cctype>

namespace lockstep::platform
{
    namespace
    {
        constexpr std::array<std::string_view, static_cast<std::size_t>(Key::Count)> k_keyNames{
            "A", "B", "C", "D", "E", "F", "G", "H", "I", "J", "K", "L", "M", "N", "O", "P", "Q", "R", "S", "T", "U",
            "V", "W", "X", "Y", "Z",
            "0", "1", "2", "3", "4", "5", "6", "7", "8", "9",
            "F1", "F2", "F3", "F4", "F5", "F6", "F7", "F8", "F9", "F10", "F11", "F12",
            "Escape", "Enter", "Tab", "Backspace", "Space", "Left", "Right", "Up", "Down",
            "Left Shift", "Left Ctrl", "Left Alt", "Right Shift", "Right Ctrl", "Right Alt",
            "Delete", "Insert", "Home", "End", "Page Up", "Page Down", "Caps Lock",
            "-", "=", "[", "]", "\\", ";", "'", ",", ".", "/", "`"};

        constexpr std::array<std::string_view, static_cast<std::size_t>(MouseButton::Count)> k_buttonNames{
            "Mouse Left", "Mouse Right", "Mouse Middle", "Mouse Back", "Mouse Forward"};

        bool equalsIgnoringCase(std::string_view left, std::string_view right)
        {
            return left.size() == right.size() && std::equal(left.begin(), left.end(), right.begin(),
                [](char a, char b)
                {
                    return std::tolower(static_cast<unsigned char>(a)) == std::tolower(static_cast<unsigned char>(b));
                });
        }

        constexpr std::size_t toIndex(Key key)
        {
            return static_cast<std::size_t>(key);
        }

        constexpr std::size_t toIndex(MouseButton button)
        {
            return static_cast<std::size_t>(button);
        }

        template<std::size_t Count>
        void setState(std::bitset<Count>& down, std::bitset<Count>& pressed, std::bitset<Count>& released,
            std::size_t index, bool isDown)
        {
            if (index >= Count || down.test(index) == isDown)
            {
                return;
            }

            down.set(index, isDown);

            if (isDown)
            {
                pressed.set(index);
            }
            else
            {
                released.set(index);
            }
        }

        template<std::size_t Count>
        bool testBit(const std::bitset<Count>& bits, std::size_t index)
        {
            return index < Count && bits.test(index);
        }
    } //namespace

    bool Input::isKeyDown(Key key) const
    {
        return testBit(m_keysDown, toIndex(key));
    }

    bool Input::wasKeyPressed(Key key) const
    {
        return testBit(m_keysPressed, toIndex(key));
    }

    bool Input::wasKeyReleased(Key key) const
    {
        return testBit(m_keysReleased, toIndex(key));
    }

    bool Input::wasKeyTyped(Key key) const
    {
        return testBit(m_keysTyped, toIndex(key));
    }

    bool Input::isMouseButtonDown(MouseButton button) const
    {
        return testBit(m_buttonsDown, toIndex(button));
    }

    bool Input::wasMouseButtonPressed(MouseButton button) const
    {
        return testBit(m_buttonsPressed, toIndex(button));
    }

    bool Input::wasMouseButtonReleased(MouseButton button) const
    {
        return testBit(m_buttonsReleased, toIndex(button));
    }

    math::Vec2 Input::mouseDelta() const
    {
        return m_mouseDelta;
    }

    math::Vec2 Input::scrollDelta() const
    {
        return m_scrollDelta;
    }

    std::optional<math::Vec2> Input::cursorPosition() const
    {
        return m_cursorPosition;
    }

    std::u32string_view Input::typedText() const
    {
        return std::u32string_view(m_typed.data(), m_typedCount);
    }

    void InputAccess::setKey(Input& input, Key key, bool down)
    {
        const std::size_t index = toIndex(key);

        if (down && index < Input::k_keyCount)
        {
            input.m_keysTyped.set(index);
        }

        setState(input.m_keysDown, input.m_keysPressed, input.m_keysReleased, index, down);
    }

    void InputAccess::setCursorPosition(Input& input, std::optional<math::Vec2> position)
    {
        input.m_cursorPosition = position;
    }

    void InputAccess::addTypedCharacter(Input& input, char32_t codepoint)
    {
        if (input.m_typedCount < input.m_typed.size())
        {
            input.m_typed[input.m_typedCount++] = codepoint;
        }
    }

    void InputAccess::setMouseButton(Input& input, MouseButton button, bool down)
    {
        setState(input.m_buttonsDown, input.m_buttonsPressed, input.m_buttonsReleased, toIndex(button), down);
    }

    void InputAccess::addMouseDelta(Input& input, math::Vec2 delta)
    {
        input.m_mouseDelta += delta;
    }

    void InputAccess::addScrollDelta(Input& input, math::Vec2 delta)
    {
        input.m_scrollDelta += delta;
    }

    void InputAccess::releaseAll(Input& input)
    {
        input.m_keysReleased |= input.m_keysDown;
        input.m_keysDown.reset();
        input.m_buttonsReleased |= input.m_buttonsDown;
        input.m_buttonsDown.reset();
    }

    void InputAccess::clearTransient(Input& input)
    {
        input.m_keysPressed.reset();
        input.m_keysReleased.reset();
        input.m_keysTyped.reset();
        input.m_buttonsPressed.reset();
        input.m_buttonsReleased.reset();
        input.m_mouseDelta = {0.0f, 0.0f};
        input.m_scrollDelta = {0.0f, 0.0f};
        input.m_typedCount = 0;
    }

    void InputAccess::accumulate(Input& into, const Input& newer)
    {
        into.m_keysDown = newer.m_keysDown;
        into.m_keysPressed |= newer.m_keysPressed;
        into.m_keysReleased |= newer.m_keysReleased;
        into.m_keysTyped |= newer.m_keysTyped;
        into.m_buttonsDown = newer.m_buttonsDown;
        into.m_buttonsPressed |= newer.m_buttonsPressed;
        into.m_buttonsReleased |= newer.m_buttonsReleased;
        into.m_mouseDelta += newer.m_mouseDelta;
        into.m_scrollDelta += newer.m_scrollDelta;
        into.m_cursorPosition = newer.m_cursorPosition;

        for (const char32_t codepoint : newer.typedText())
        {
            addTypedCharacter(into, codepoint);
        }
    }

    void InputAccess::hideKeyboard(Input& input)
    {
        input.m_keysDown.reset();
        input.m_keysPressed.reset();
        input.m_keysReleased.reset();
        input.m_keysTyped.reset();
        input.m_typedCount = 0;
    }

    void InputAccess::hideMouse(Input& input)
    {
        input.m_buttonsDown.reset();
        input.m_buttonsPressed.reset();
        input.m_buttonsReleased.reset();
        input.m_mouseDelta = {0.0f, 0.0f};
        input.m_scrollDelta = {0.0f, 0.0f};
        input.m_cursorPosition.reset();
    }

    std::string_view keyName(Key key)
    {
        const std::size_t index = toIndex(key);

        return index < k_keyNames.size() ? k_keyNames[index] : std::string_view("?");
    }

    std::string_view mouseButtonName(MouseButton button)
    {
        const std::size_t index = toIndex(button);

        return index < k_buttonNames.size() ? k_buttonNames[index] : std::string_view("?");
    }

    std::optional<Key> keyFromName(std::string_view name)
    {
        for (std::size_t index = 0; index < k_keyNames.size(); ++index)
        {
            if (equalsIgnoringCase(k_keyNames[index], name))
            {
                return static_cast<Key>(index);
            }
        }

        return std::nullopt;
    }

    std::optional<MouseButton> mouseButtonFromName(std::string_view name)
    {
        for (std::size_t index = 0; index < k_buttonNames.size(); ++index)
        {
            if (equalsIgnoringCase(k_buttonNames[index], name))
            {
                return static_cast<MouseButton>(index);
            }
        }

        return std::nullopt;
    }
} //namespace lockstep::platform
