#ifndef LOCKSTEP_GAME_WEAPONS_SHOT_AIM_HPP
#define LOCKSTEP_GAME_WEAPONS_SHOT_AIM_HPP

#include "Player/PlayerCommand.hpp"

#include <Lockstep/Core/Math.hpp>

#include <cstddef>
#include <cstdint>

namespace game
{
    namespace math = lockstep::math;

    // Where shots go relative to the look. Host and client compute shot directions with the same
    // functions from the same command, so the client's impacts match the host's hits.
    // hip:          cone half-angle of a hip-fire shot standing still, radians.
    // aim:          cone half-angle aiming down sights, radians.
    // moving:       added at full move input (hip fire only), radians.
    // sprinting:    added while running (hip fire only), radians.
    // crouchScale:  cone multiplier while crouched.
    // maxAimOffset: largest sight offset from the look the host accepts, radians (sway, weapon
    //               kick and head motion stay well inside it; anything beyond is clamped).
    struct SpreadSettings
    {
        float hip{0.018f};
        float aim{0.0012f};
        float moving{0.016f};
        float sprinting{0.03f};
        float crouchScale{0.7f};
        float maxAimOffset{0.14f};
    };

    // yaw, pitch: the look (as in PlayerCommand).
    // offset:     (yaw, pitch) turn relative to the look, radians.
    // Returns:    unit direction of the look turned by the offset in its own frame.
    [[nodiscard]] math::Vec3 offsetDirection(float yaw, float pitch, const math::Vec2& offset);

    // The inverse of offsetDirection().
    // yaw, pitch: the look.
    // direction:  a unit world direction.
    // Returns:    (yaw, pitch) offset that turns the look onto `direction`.
    [[nodiscard]] math::Vec2 aimOffsetTowards(float yaw, float pitch, const math::Vec3& direction);

    // offset:   a sight offset from a command.
    // settings: the accepted limit.
    // Returns:  the offset shortened to SpreadSettings::maxAimOffset.
    [[nodiscard]] math::Vec2 clampAimOffset(const math::Vec2& offset, const SpreadSettings& settings);

    // command:  the shooter's command (aim, crouch, move, sprint).
    // settings: the cone sizes.
    // Returns:  the cone half-angle of its shots, radians.
    [[nodiscard]] float shotSpread(const PlayerCommand& command, const SpreadSettings& settings);

    // The direction one shot leaves in: the look, turned by the command's sight offset (clamped),
    // then scattered inside the spread cone by a hash of the shooter and the shot number.
    // command:    the shooter's command.
    // pawnIndex:  the shooter (0 host, 1 client).
    // shotNumber: the shot counter value of this shot (wrapping).
    // settings:   the cone sizes and the offset limit.
    // Returns:    unit direction.
    [[nodiscard]] math::Vec3 shotDirection(const PlayerCommand& command, std::size_t pawnIndex, uint8_t shotNumber,
        const SpreadSettings& settings);
} //namespace game

#endif //LOCKSTEP_GAME_WEAPONS_SHOT_AIM_HPP
