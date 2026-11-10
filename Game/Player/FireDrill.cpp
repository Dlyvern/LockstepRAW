#include "Player/FireDrill.hpp"

#if LOCKSTEP_DEV_TOOLS

#include <algorithm>
#include <cmath>
#include <string_view>

namespace game
{
    namespace
    {
        constexpr std::string_view k_drillFlag{"--dev-fire-drill"};
        constexpr std::string_view k_scopeDrillFlag{"--dev-scope-drill"};
        constexpr float k_scopeRoundSeconds{10.0f};
        constexpr float k_scopeTurnPerRound{-0.9f};
        constexpr float k_scopeSweep{0.0015f};
        constexpr float k_scopeShotSeconds[] = {5.0f, 5.3f, 5.6f};
        constexpr float k_roundSeconds{8.0f};
        constexpr float k_turnPerRound{0.6f};
        constexpr float k_clickSeconds[] = {0.5f, 0.8f, 1.1f};

        bool isBetween(float seconds, float from, float to)
        {
            return seconds >= from && seconds < to;
        }

        bool isPressTick(float seconds, float at, float delta)
        {
            return seconds >= at && seconds < at + delta;
        }
    } //namespace

    bool isFireDrillRequested(std::span<const char* const> arguments)
    {
        return std::any_of(arguments.begin(), arguments.end(), [](const char* argument)
        {
            return argument != nullptr && std::string_view(argument) == k_drillFlag;
        });
    }

    bool isScopeDrillRequested(std::span<const char* const> arguments)
    {
        return std::any_of(arguments.begin(), arguments.end(), [](const char* argument)
        {
            return argument != nullptr && std::string_view(argument) == k_scopeDrillFlag;
        });
    }

    FireButtons scopeDrillButtons(LocalControls& controls, uint64_t tick, float delta)
    {
        const double elapsed = static_cast<double>(tick) * static_cast<double>(delta);
        const auto round = static_cast<uint64_t>(elapsed / k_scopeRoundSeconds);
        const auto seconds = static_cast<float>(elapsed - static_cast<double>(round) * k_scopeRoundSeconds);
        controls.isFlashlightOn = false;
        controls.wantsLowered = false;

        if (isPressTick(seconds, 0.0f, delta))
        {
            controls.yaw = std::remainder(controls.yaw + k_scopeTurnPerRound, 2.0f * math::k_pi);
        }

        if (isBetween(seconds, 3.0f, 4.5f))
        {
            controls.yaw += k_scopeSweep * std::sin((seconds - 3.0f) * 2.0f * math::k_pi / 1.5f);
        }

        if (seconds < 1.0f)
        {
            return FireButtons{};
        }

        if (seconds < 7.0f)
        {
            controls.trigger.mode = FireMode::Semi;
            const bool isClick = std::any_of(std::begin(k_scopeShotSeconds), std::end(k_scopeShotSeconds),
                [seconds, delta](float at) { return isPressTick(seconds, at, delta); });
            return FireButtons{.isPressed = isClick, .isHeld = isClick, .isAiming = true};
        }

        controls.trigger.mode = FireMode::Auto;
        return FireButtons{.isPressed = isPressTick(seconds, 7.0f, delta), .isHeld = isBetween(seconds, 7.0f, 7.3f),
            .isAiming = seconds < 9.0f};
    }

    FireButtons fireDrillButtons(LocalControls& controls, uint64_t tick, float delta)
    {
        const double elapsed = static_cast<double>(tick) * static_cast<double>(delta);
        const auto round = static_cast<uint64_t>(elapsed / k_roundSeconds);
        const auto seconds = static_cast<float>(elapsed - static_cast<double>(round) * k_roundSeconds);
        controls.isFlashlightOn = true;
        controls.wantsLowered = false;

        if (isPressTick(seconds, 0.0f, delta))
        {
            controls.yaw = std::remainder(controls.yaw + k_turnPerRound, 2.0f * math::k_pi);
        }

        if (seconds < 1.5f)
        {
            controls.trigger.mode = FireMode::Semi;
            const bool isClick = std::any_of(std::begin(k_clickSeconds), std::end(k_clickSeconds),
                [seconds, delta](float at) { return isPressTick(seconds, at, delta); });
            return FireButtons{.isPressed = isClick, .isHeld = isClick};
        }

        if (seconds < 3.2f)
        {
            controls.trigger.mode = FireMode::Auto;
            const bool isFiring = isBetween(seconds, 2.0f, 3.0f);
            return FireButtons{.isPressed = isPressTick(seconds, 2.0f, delta), .isHeld = isFiring, .isAiming = true};
        }

        if (seconds < 4.5f)
        {
            controls.trigger.mode = FireMode::Burst;
            return FireButtons{.isPressed = isPressTick(seconds, 3.8f, delta), .isHeld = isBetween(seconds, 3.8f, 3.9f),
                .isAiming = true};
        }

        controls.trigger.mode = FireMode::Auto;
        return FireButtons{.isPressed = isPressTick(seconds, 5.0f, delta), .isHeld = isBetween(seconds, 5.0f, 6.0f)};
    }
} //namespace game

#endif
