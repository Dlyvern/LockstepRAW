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
