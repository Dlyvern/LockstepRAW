#include "Weapons/ShotAim.hpp"

#include <algorithm>
#include <cmath>

namespace game
{
    namespace
    {
        constexpr math::Vec3 k_up{0.0f, 1.0f, 0.0f};
        constexpr math::Vec3 k_right{1.0f, 0.0f, 0.0f};
        constexpr uint32_t k_spreadSalt{0x5BD1E995u};

        math::Quat lookRotation(float yaw, float pitch)
        {
            return math::angleAxis(yaw, k_up) * math::angleAxis(pitch, k_right);
        }

        uint32_t mix(uint32_t value)
        {
            value ^= value >> 16;
            value *= 0x7FEB352Du;
            value ^= value >> 15;
            value *= 0x846CA68Bu;
            value ^= value >> 16;
            return value;
        }

        float unit(uint32_t value)
        {
            return static_cast<float>(value & 0xFFFFu) / 65535.0f;
        }
    } //namespace

    math::Vec3 offsetDirection(float yaw, float pitch, const math::Vec2& offset)
    {
        const float horizontal = std::cos(offset.y);
        const math::Vec3 local{-std::sin(offset.x) * horizontal, std::sin(offset.y), -std::cos(offset.x) * horizontal};

        return math::normalize(lookRotation(yaw, pitch) * local);
    }

    math::Vec2 aimOffsetTowards(float yaw, float pitch, const math::Vec3& direction)
    {
        const math::Vec3 local = math::conjugate(lookRotation(yaw, pitch)) * math::normalize(direction);

        return math::Vec2{std::atan2(-local.x, -local.z), std::asin(math::clamp(local.y, -1.0f, 1.0f))};
    }

    math::Vec2 clampAimOffset(const math::Vec2& offset, const SpreadSettings& settings)
    {
        const float length = math::length(offset);

        if (!std::isfinite(length))
        {
            return math::Vec2{0.0f, 0.0f};
        }

        return length > settings.maxAimOffset ? offset * (settings.maxAimOffset / length) : offset;
    }

    float shotSpread(const PlayerCommand& command, const SpreadSettings& settings)
    {
        const float crouch = command.crouch ? settings.crouchScale : 1.0f;

        if (command.aim)
        {
            return settings.aim * crouch;
        }

        const float moving = std::min(math::length(command.move), 1.0f) * settings.moving;
        const float running = command.sprint && command.move.y > 0.0f ? settings.sprinting : 0.0f;

        return (settings.hip + moving + running) * crouch;
    }

    math::Vec3 shotDirection(const PlayerCommand& command, std::size_t pawnIndex, uint8_t shotNumber,
        const SpreadSettings& settings)
    {
        const uint32_t seed = mix(static_cast<uint32_t>(shotNumber) * 2654435761u
            + static_cast<uint32_t>(pawnIndex) * k_spreadSalt);
        const float angle = 2.0f * math::k_pi * unit(seed);
        const float radius = shotSpread(command, settings) * std::sqrt(unit(mix(seed)));
        const math::Vec2 scatter{std::cos(angle) * radius, std::sin(angle) * radius};

        return offsetDirection(command.yaw, command.pitch, clampAimOffset(command.aimOffset, settings) + scatter);
    }
} //namespace game
