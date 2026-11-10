#ifndef LOCKSTEP_GAME_PLAYER_KEY_BINDINGS_HPP
#define LOCKSTEP_GAME_PLAYER_KEY_BINDINGS_HPP

#include <Lockstep/Platform/Input.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace game
{
    // The player actions the keyboard and mouse drive (each can be rebound in the options).
    enum class Action : uint8_t
    {
        MoveForward = 0,
        MoveBack,
        MoveLeft,
        MoveRight,
        Sprint,
        Crouch,
        CrouchToggle,
        Jump,
        LeanLeft,
        LeanRight,
        Kick,
        Reload,
        Interact,
        Flashlight,
        FireMode,
        LowerWeapon,
        Weapon1,
        Weapon2,
        Weapon3,
        BulletTime,
        Fire,
        Aim,
        Count
    };

    inline constexpr std::size_t k_actionCount{static_cast<std::size_t>(Action::Count)};

    // One key or mouse button; neither set means unbound.
    struct Binding
    {
        std::optional<lockstep::platform::Key> key;
        std::optional<lockstep::platform::MouseButton> button;
    };

    // The two bindings of one action; either one triggers it.
    struct ActionBinding
    {
        Binding primary{};
        Binding secondary{};
    };

    // What every action is bound to.
    struct KeyBindings
    {
        std::array<ActionBinding, k_actionCount> actions{};
    };

    // Returns: the default bindings (WASD, Shift, Ctrl / C, Space, Q / E, V, R, F, B, X, 1-3,
    //          Left Alt / middle mouse, left and right mouse).
    [[nodiscard]] KeyBindings defaultKeyBindings();

    // bindings: the bindings.
    // action:   the action.
    // Returns:  that action's bindings.
    [[nodiscard]] const ActionBinding& actionBinding(const KeyBindings& bindings, Action action);

    // Returns: true if a binding of `action` is held.
    // bindings: the bindings.
    // input:    the input snapshot.
    // action:   the action.
    [[nodiscard]] bool isActionDown(const KeyBindings& bindings, const lockstep::platform::Input& input, Action action);

    // Returns: true if a binding of `action` went down during the snapshot.
    // bindings: the bindings.
    // input:    the input snapshot.
    // action:   the action.
    [[nodiscard]] bool wasActionPressed(const KeyBindings& bindings, const lockstep::platform::Input& input,
        Action action);

    // Returns: +1 when only `positive` is held, -1 when only `negative` is, else 0.
    // bindings: the bindings.
    // input:    the input snapshot.
    // positive: the action for +1.
    // negative: the action for -1.
    [[nodiscard]] float actionAxis(const KeyBindings& bindings, const lockstep::platform::Input& input, Action positive,
        Action negative);

    // action:  an action.
    // Returns: its name for the options menu ("Move forward").
    [[nodiscard]] std::string_view actionLabel(Action action);

    // action:  an action.
    // Returns: its key in the settings file ("move_forward").
    [[nodiscard]] std::string_view actionSettingsKey(Action action);

    // binding: a binding.
    // Returns: its name ("W", "Mouse Left"), "-" when unbound.
    [[nodiscard]] std::string bindingName(const Binding& binding);

    // text:    a name as bindingName() writes it.
    // Returns: the binding (unbound for "-"), or nothing if the name is unknown.
    [[nodiscard]] std::optional<Binding> parseBinding(std::string_view text);
} //namespace game

#endif //LOCKSTEP_GAME_PLAYER_KEY_BINDINGS_HPP
