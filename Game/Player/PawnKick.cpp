#include "Player/PawnKick.hpp"

#include <algorithm>

namespace game
{
    namespace
    {
        uint8_t kickLengthTicks(const PawnSettings& settings, float delta)
        {
            return std::max(secondsToTicks(settings.kick.seconds, delta), uint8_t{1});
        }

        bool canStartKick(const PawnMotion& motion)
        {
            return (motion.isGrounded || motion.coyoteTicks > 0) && motion.stance != PawnStance::Slide
                && !isSliding(motion) && !isMantling(motion) && motion.kickCooldownTicks == 0;
        }
    } //namespace

    void updateKick(PawnMotion& motion, const PlayerCommand& command, const PawnSettings& settings, float delta)
    {
        const bool isPressed = command.kick && !motion.isKickHeld;
        motion.isKickHeld = command.kick;

        if (motion.kickTicks > 0)
        {
            ++motion.kickTicks;

            if (motion.kickTicks > kickLengthTicks(settings, delta))
            {
                motion.kickTicks = 0;
                motion.kickCooldownTicks = secondsToTicks(settings.kick.cooldownSeconds, delta);
            }
        }
        else if (motion.kickCooldownTicks > 0)
        {
            --motion.kickCooldownTicks;
        }
        else if (isPressed && canStartKick(motion))
        {
            motion.kickTicks = 1;
        }

        if (motion.kickTicks > 0)
        {
            motion.sprintRecoverTicks = std::max(motion.sprintRecoverTicks,
                std::max(secondsToTicks(settings.sprintRecoverSeconds, delta), uint8_t{1}));
        }
    }

    bool isKicking(const PawnMotion& motion)
    {
        return motion.kickTicks > 0;
    }

    bool isKickImpact(const PawnMotion& motion, const PawnSettings& settings, float delta)
    {
        return motion.kickTicks > 0
            && motion.kickTicks == std::max(secondsToTicks(settings.kick.impactSeconds, delta), uint8_t{1});
    }

    float kickSeconds(const PawnMotion& motion, float delta)
    {
        return motion.kickTicks > 0 ? static_cast<float>(motion.kickTicks - 1) * delta : -1.0f;
    }

    float kickSpeedScale(const PawnMotion& motion, const PawnSettings& settings)
    {
        return isKicking(motion) ? settings.kick.moveScale : 1.0f;
    }
} //namespace game
