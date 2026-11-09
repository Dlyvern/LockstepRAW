#include "Lockstep/Platform/Input.hpp"

#include "InputAccess.hpp"

namespace lockstep::platform
{
    namespace
    {
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

    void InputAccess::setKey(Input& input, Key key, bool down)
    {
        setState(input.m_keysDown, input.m_keysPressed, input.m_keysReleased, toIndex(key), down);
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
        input.m_buttonsPressed.reset();
        input.m_buttonsReleased.reset();
        input.m_mouseDelta = {0.0f, 0.0f};
        input.m_scrollDelta = {0.0f, 0.0f};
    }

    void InputAccess::accumulate(Input& into, const Input& newer)
    {
        into.m_keysDown = newer.m_keysDown;
        into.m_keysPressed |= newer.m_keysPressed;
        into.m_keysReleased |= newer.m_keysReleased;
        into.m_buttonsDown = newer.m_buttonsDown;
        into.m_buttonsPressed |= newer.m_buttonsPressed;
        into.m_buttonsReleased |= newer.m_buttonsReleased;
        into.m_mouseDelta += newer.m_mouseDelta;
        into.m_scrollDelta += newer.m_scrollDelta;
    }

    void InputAccess::hideKeyboard(Input& input)
    {
        input.m_keysDown.reset();
        input.m_keysPressed.reset();
        input.m_keysReleased.reset();
    }

    void InputAccess::hideMouse(Input& input)
    {
        input.m_buttonsDown.reset();
        input.m_buttonsPressed.reset();
        input.m_buttonsReleased.reset();
        input.m_mouseDelta = {0.0f, 0.0f};
        input.m_scrollDelta = {0.0f, 0.0f};
    }
} //namespace lockstep::platform
