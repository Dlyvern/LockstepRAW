#include "Player/KeyBindings.hpp"

namespace game
{
    namespace
    {
        namespace platform = lockstep::platform;

        using platform::Key;
        using platform::MouseButton;

        constexpr std::string_view k_unboundName{"-"};

        struct ActionInfo
        {
            std::string_view label;
            std::string_view settingsKey;
            ActionBinding defaults;
        };

        constexpr Binding key(Key value)
        {
            return Binding{.key = value, .button = std::nullopt};
        }

        constexpr Binding mouse(MouseButton value)
        {
            return Binding{.key = std::nullopt, .button = value};
        }

        const std::array<ActionInfo, k_actionCount> k_actions{{
            {"Move forward", "move_forward", {key(Key::W), {}}},
            {"Move back", "move_back", {key(Key::S), {}}},
            {"Move left", "move_left", {key(Key::A), {}}},
            {"Move right", "move_right", {key(Key::D), {}}},
            {"Sprint", "sprint", {key(Key::LeftShift), key(Key::RightShift)}},
            {"Crouch (hold)", "crouch", {key(Key::LeftControl), key(Key::RightControl)}},
            {"Crouch (toggle)", "crouch_toggle", {key(Key::C), {}}},
            {"Jump / vault", "jump", {key(Key::Space), {}}},
            {"Lean left", "lean_left", {key(Key::Q), {}}},
            {"Lean right", "lean_right", {key(Key::E), {}}},
            {"Kick", "kick", {key(Key::V), {}}},
            {"Reload", "reload", {key(Key::R), {}}},
            {"Interact / revive", "interact", {key(Key::F), {}}},
            {"Flashlight", "flashlight", {key(Key::F), {}}},
            {"Fire mode", "fire_mode", {key(Key::B), {}}},
            {"Lower weapon", "lower_weapon", {key(Key::X), {}}},
            {"Weapon 1 (M4)", "weapon_1", {key(Key::Num1), {}}},
            {"Weapon 2 (Glock)", "weapon_2", {key(Key::Num2), {}}},
            {"Weapon 3 (KRISS)", "weapon_3", {key(Key::Num3), {}}},
            {"Bullet-time", "bullet_time", {key(Key::LeftAlt), mouse(MouseButton::Middle)}},
            {"Fire", "fire", {mouse(MouseButton::Left), {}}},
            {"Aim down sights", "aim", {mouse(MouseButton::Right), {}}}}};

        const ActionInfo& info(Action action)
        {
            return k_actions[static_cast<std::size_t>(action) % k_actionCount];
        }

        bool isDown(const platform::Input& input, const Binding& binding)
        {
            return (binding.key && input.isKeyDown(*binding.key))
                || (binding.button && input.isMouseButtonDown(*binding.button));
        }

        bool wasPressed(const platform::Input& input, const Binding& binding)
        {
            return (binding.key && input.wasKeyPressed(*binding.key))
                || (binding.button && input.wasMouseButtonPressed(*binding.button));
        }
    } //namespace

    KeyBindings defaultKeyBindings()
    {
        KeyBindings bindings{};

        for (std::size_t index = 0; index < k_actionCount; ++index)
        {
            bindings.actions[index] = k_actions[index].defaults;
        }

        return bindings;
    }

    const ActionBinding& actionBinding(const KeyBindings& bindings, Action action)
    {
        return bindings.actions[static_cast<std::size_t>(action) % k_actionCount];
    }

    bool isActionDown(const KeyBindings& bindings, const platform::Input& input, Action action)
    {
        const ActionBinding& binding = actionBinding(bindings, action);

        return isDown(input, binding.primary) || isDown(input, binding.secondary);
    }

    bool wasActionPressed(const KeyBindings& bindings, const platform::Input& input, Action action)
    {
        const ActionBinding& binding = actionBinding(bindings, action);

        return wasPressed(input, binding.primary) || wasPressed(input, binding.secondary);
    }

    float actionAxis(const KeyBindings& bindings, const platform::Input& input, Action positive, Action negative)
    {
        return (isActionDown(bindings, input, positive) ? 1.0f : 0.0f)
            - (isActionDown(bindings, input, negative) ? 1.0f : 0.0f);
    }

    std::string_view actionLabel(Action action)
    {
        return info(action).label;
    }

    std::string_view actionSettingsKey(Action action)
    {
        return info(action).settingsKey;
    }

    std::string bindingName(const Binding& binding)
    {
        if (binding.key)
        {
            return std::string(platform::keyName(*binding.key));
        }

        if (binding.button)
        {
            return std::string(platform::mouseButtonName(*binding.button));
        }

        return std::string(k_unboundName);
    }

    std::optional<Binding> parseBinding(std::string_view text)
    {
        if (text == k_unboundName)
        {
            return Binding{};
        }

        if (const std::optional<Key> parsed = platform::keyFromName(text))
        {
            return key(*parsed);
        }

        if (const std::optional<MouseButton> parsed = platform::mouseButtonFromName(text))
        {
            return mouse(*parsed);
        }

        return std::nullopt;
    }
} //namespace game
