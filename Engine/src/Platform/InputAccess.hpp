#ifndef LOCKSTEP_PLATFORM_INPUT_ACCESS_HPP
#define LOCKSTEP_PLATFORM_INPUT_ACCESS_HPP

#include "Lockstep/Core/Math.hpp"
#include "Lockstep/Platform/Input.hpp"

#include <optional>

namespace lockstep::platform
{
    // Engine-internal writes to an Input snapshot. Game only reads Input; the Window and the run
    // loop build and combine snapshots through these functions.
    struct InputAccess
    {
        // Marks a key as down or up and records the change. A press of a key that is already
        // down (key repeat) only marks it as typed again (Input::wasKeyTyped()).
        // input: the snapshot to change.
        // key:   the key.
        // down:  true for pressed, false for released.
        static void setKey(Input& input, Key key, bool down);

        // Marks a mouse button as down or up and records the change, like setKey().
        // input:  the snapshot to change.
        // button: the button.
        // down:   true for pressed, false for released.
        static void setMouseButton(Input& input, MouseButton button, bool down);

        // Adds cursor movement.
        // input: the snapshot to change.
        // delta: movement in window pixels.
        static void addMouseDelta(Input& input, math::Vec2 delta);

        // Adds mouse wheel movement.
        // input: the snapshot to change.
        // delta: wheel movement.
        static void addScrollDelta(Input& input, math::Vec2 delta);

        // Sets where the cursor is.
        // input:    the snapshot to change.
        // position: normalized window position, or nothing when unknown or captured.
        static void setCursorPosition(Input& input, std::optional<math::Vec2> position);

        // Appends one typed character; dropped once k_maxTypedCharacters are kept.
        // input:     the snapshot to change.
        // codepoint: the Unicode code point.
        static void addTypedCharacter(Input& input, char32_t codepoint);

        // Releases every held key and button, recording the releases. Used when the window
        // loses focus, since the OS stops reporting key-up events then.
        // input: the snapshot to change.
        static void releaseAll(Input& input);

        // Clears the recorded changes and movement, keeping what is held.
        // input: the snapshot to change.
        static void clearTransient(Input& input);

        // Folds a newer snapshot into an older one: held state comes from `newer`, changes and
        // movement add up.
        // into:  the older snapshot, updated in place.
        // newer: the snapshot that follows it.
        static void accumulate(Input& into, const Input& newer);

        // Hides all keyboard state, as if no key were held or changed.
        // input: the snapshot to change.
        static void hideKeyboard(Input& input);

        // Hides all mouse state: buttons, movement and wheel.
        // input: the snapshot to change.
        static void hideMouse(Input& input);
    };
} //namespace lockstep::platform

#endif //LOCKSTEP_PLATFORM_INPUT_ACCESS_HPP
