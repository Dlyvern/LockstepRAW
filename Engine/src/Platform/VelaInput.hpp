#ifndef LOCKSTEP_PLATFORM_VELA_INPUT_HPP
#define LOCKSTEP_PLATFORM_VELA_INPUT_HPP

#include "Lockstep/Platform/Input.hpp"

#include <Vela/Core/Input.hpp>

#include <optional>

namespace lockstep::platform
{
    // key:     a Vela key.
    // Returns: the matching Engine key, or nothing if the Engine does not report that key.
    [[nodiscard]] std::optional<Key> fromVelaKey(vela::core::Key key);

    // button:  a Vela mouse button.
    // Returns: the matching Engine button, or nothing if the Engine does not report that button.
    [[nodiscard]] std::optional<MouseButton> fromVelaMouseButton(vela::core::MouseButton button);
} //namespace lockstep::platform

#endif //LOCKSTEP_PLATFORM_VELA_INPUT_HPP
