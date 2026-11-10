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
        constexpr std::string_view k_vaultDrillFlag{"--dev-vault-drill"};
        constexpr float k_vaultRoundSeconds{6.0f};
        constexpr float k_barrierZ{12.05f};
        constexpr float k_laneX{40.0f};
        constexpr float k_laneStep{3.3f};
        constexpr float k_sideDistance{4.0f};
        constexpr float k_arriveDistance{0.4f};
        constexpr float k_jumpDistance{1.0f};
        constexpr float k_jumpPerSpeed{0.08f};
        constexpr float k_slideDistance{3.2f};
        constexpr float k_stagingDistance{6.0f};
        constexpr float k_stagingX{28.0f};
        constexpr float k_laneTolerance{0.6f};
        constexpr std::string_view k_kickDrillFlag{"--dev-kick-drill"};
        constexpr std::string_view k_watchNpcFlag{"--dev-watch-npc"};
        constexpr float k_kickInterval{1.2f};
        constexpr float k_kickPressSeconds{0.05f};
        constexpr float k_kickDrillPitch{-0.15f};
        constexpr float k_kickApproach{0.9f};
        constexpr float k_pitchSweep{1.25f};
        constexpr float k_pitchSweepSeconds{7.0f};

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

    bool isVaultDrillRequested(std::span<const char* const> arguments)
    {
        return std::any_of(arguments.begin(), arguments.end(), [](const char* argument)
        {
            return argument != nullptr && std::string_view(argument) == k_vaultDrillFlag;
        });
    }

    void applyVaultDrill(PlayerCommand& command, const math::Vec3& foot, uint64_t tick, float delta)
    {
        const double elapsed = static_cast<double>(tick) * static_cast<double>(delta);
        const auto round = static_cast<uint64_t>(elapsed / k_vaultRoundSeconds);
        const float laneX = k_laneX + k_laneStep * static_cast<float>((round / 2) % 4);
        const float nearSide = k_barrierZ + (foot.z > k_barrierZ ? k_sideDistance : -k_sideDistance);
        const bool isStaging = foot.z > k_barrierZ + k_stagingDistance;
        const bool isInLane = std::abs(foot.x - laneX) < k_laneTolerance;
        float targetX = isStaging ? k_stagingX : laneX;
        float targetZ = isStaging ? k_barrierZ + k_sideDistance : nearSide;

        if (!isStaging && isInLane)
        {
            targetZ = k_barrierZ + (round % 2 == 0 ? -k_sideDistance : k_sideDistance);
        }

        const math::Vec3 toTarget{targetX - foot.x, 0.0f, targetZ - foot.z};
        const float distance = math::length(toTarget);
        const uint64_t style = round % 6;
        const bool isSprint = style >= 2;
        const bool isSlide = style >= 4;

        command.move = math::Vec2{0.0f, distance > k_arriveDistance ? 1.0f : 0.0f};
        command.sprint = isSprint;
        command.crouch = false;
        command.jump = false;

        if (distance > k_arriveDistance)
        {
            command.yaw = std::atan2(-toTarget.x, -toTarget.z);
        }

        const float toBarrier = std::abs(foot.z - k_barrierZ);
        const bool isApproaching = isInLane && !isStaging && (targetZ - k_barrierZ) * (k_barrierZ - foot.z) > 0.0f;
        const float speed = isSprint ? 6.0f : 3.8f;

        if (isApproaching && isSlide && toBarrier < k_slideDistance)
        {
            command.crouch = true;
        }

        if (isApproaching && toBarrier < k_jumpDistance + k_jumpPerSpeed * speed)
        {
            command.jump = (tick % 8) < 4;
        }
    }

    bool isKickDrillRequested(std::span<const char* const> arguments)
    {
        return std::any_of(arguments.begin(), arguments.end(), [](const char* argument)
        {
            return argument != nullptr && std::string_view(argument) == k_kickDrillFlag;
        });
    }

    void applyKickDrill(PlayerCommand& command, uint64_t tick, float delta, const math::Vec3& foot,
        const std::optional<math::Vec3>& target)
    {
        if (target)
        {
            const math::Vec3 offset{target->x - foot.x, 0.0f, target->z - foot.z};
            command.yaw = std::atan2(-offset.x, -offset.z);
            command.move = math::Vec2{0.0f, math::length(offset) > k_kickApproach ? 1.0f : 0.0f};
        }

        const double elapsed = static_cast<double>(tick) * static_cast<double>(delta);
        const double phase = std::fmod(elapsed, static_cast<double>(k_kickInterval));
        command.kick = phase < static_cast<double>(k_kickPressSeconds);
        const double sweep = 0.5 - 0.5 * std::cos(2.0 * static_cast<double>(lockstep::math::k_pi) * elapsed
            / static_cast<double>(k_pitchSweepSeconds));
        command.pitch = k_kickDrillPitch - k_pitchSweep * static_cast<float>(sweep);
    }

    bool isWatchNpcRequested(std::span<const char* const> arguments)
    {
        return std::any_of(arguments.begin(), arguments.end(), [](const char* argument)
        {
            return argument != nullptr && std::string_view(argument) == k_watchNpcFlag;
        });
    }

    void lookAtPoint(PlayerCommand& command, const math::Vec3& eye, const math::Vec3& target)
    {
        const math::Vec3 offset = target - eye;
        const float planar = std::sqrt(offset.x * offset.x + offset.z * offset.z);
        command.yaw = std::atan2(-offset.x, -offset.z);
        command.pitch = std::atan2(offset.y, std::max(planar, 0.01f));
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
