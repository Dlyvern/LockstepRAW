#include "Player/MoveDrill.hpp"

#if LOCKSTEP_DEV_TOOLS

#include <Lockstep/Core/Math.hpp>

#include <algorithm>
#include <cmath>
#include <iostream>
#include <string_view>

namespace game
{
    namespace
    {
        constexpr std::string_view k_drillFlag{"--dev-move-drill"};
        constexpr float k_roundSeconds{12.0f};
        constexpr float k_turnRate{2.5f};

        bool isBetween(float seconds, float from, float to)
        {
            return seconds >= from && seconds < to;
        }
    } //namespace

    bool isMoveDrillRequested(std::span<const char* const> arguments)
    {
        return std::any_of(arguments.begin(), arguments.end(), [](const char* argument)
        {
            return argument != nullptr && std::string_view(argument) == k_drillFlag;
        });
    }

    void applyMoveDrill(PlayerCommand& command, uint64_t tick, float delta)
    {
        const double elapsed = static_cast<double>(tick) * static_cast<double>(delta);
        const auto round = static_cast<uint64_t>(elapsed / k_roundSeconds);
        const auto seconds = static_cast<float>(elapsed - static_cast<double>(round) * k_roundSeconds);

        command.move = math::Vec2{0.0f, 0.0f};
        command.sprint = false;
        command.crouch = false;
        command.jump = false;
        command.yaw = (round % 2 == 0) ? 0.0f : lockstep::math::k_pi;

        if (isBetween(seconds, 0.0f, 2.0f))
        {
            command.move = math::Vec2{0.0f, 1.0f};
        }
        else if (isBetween(seconds, 2.0f, 4.3f))
        {
            command.move = math::Vec2{0.0f, 1.0f};
            command.sprint = true;
            command.crouch = seconds >= 3.5f;
        }
        else if (isBetween(seconds, 5.0f, 6.0f))
        {
            command.move = math::Vec2{1.0f, 0.0f};
        }
        else if (isBetween(seconds, 6.0f, 7.0f))
        {
            command.move = math::Vec2{-1.0f, 0.0f};
        }
        else if (isBetween(seconds, 7.0f, 8.0f))
        {
            command.move = math::Vec2{0.0f, -1.0f};
        }
        else if (isBetween(seconds, 8.0f, 9.0f))
        {
            command.move = math::Vec2{0.0f, 1.0f};
            command.sprint = true;
            command.jump = isBetween(seconds, 8.4f, 8.5f);
        }
        else if (isBetween(seconds, 9.0f, 10.0f))
        {
            command.move = math::Vec2{0.0f, 1.0f};
            command.sprint = true;
            command.yaw += k_turnRate * (seconds - 9.0f);
        }
        else if (seconds >= 10.0f)
        {
            command.yaw += k_turnRate;
        }
    }

    void logMoveDrill(const Prediction& prediction, uint64_t tick, float delta)
    {
        const auto ticksPerRound = static_cast<uint64_t>(std::lround(k_roundSeconds / delta));

        if (ticksPerRound == 0 || tick % ticksPerRound != 0)
        {
            return;
        }

        std::clog << "Game: move drill: " << prediction.corrections << " corrections, last " << prediction.lastError
                  << " m (" << prediction.lastReplayed << " replayed), smoothing "
                  << math::length(prediction.visualOffset) << " m" << std::endl;
    }
} //namespace game

#endif
