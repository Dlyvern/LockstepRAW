#include "Player/PawnLean.hpp"

#include "Player/PawnRays.hpp"

#include <Lockstep/Physics/PhysicsWorld.hpp>

#include <algorithm>
#include <cmath>
#include <optional>

namespace game
{
    namespace
    {
        namespace physics = lockstep::physics;

        constexpr math::Vec3 k_upAxis{0.0f, 1.0f, 0.0f};
        constexpr float k_shoulderBelowEye{0.35f};
        constexpr float k_minLeanDistance{0.01f};

        float allowedLean(const PawnMotion& motion, physics::ControllerId controller,
            const physics::PhysicsWorld& physics, float side, float yaw, const PawnSettings& settings,
            std::span<const physics::BodyId> ignored)
        {
            const std::optional<math::Vec3> foot = physics.footPosition(controller);

            if (!foot)
            {
                return 0.0f;
            }

            const float eyeHeight = stanceHeight(motion.stance, settings) - k_pawnEyeBelowTop;
            const math::Vec3 right{std::cos(yaw), 0.0f, -std::sin(yaw)};
            const math::Vec3 direction = right * side;
            const float reach = settings.leanDistance + settings.leanClearance;
            float free = reach;

            for (const float height : {eyeHeight, eyeHeight - k_shoulderBelowEye})
            {
                if (const auto hit = sharedRaycast(physics, *foot + k_upAxis * height, direction, reach, controller,
                        ignored))
                {
                    free = std::min(free, hit->distance);
                }
            }

            return math::clamp((free - settings.leanClearance) / std::max(settings.leanDistance, k_minLeanDistance),
                0.0f, 1.0f);
        }
    } //namespace

    void updateLean(PawnMotion& motion, physics::ControllerId controller, const physics::PhysicsWorld& physics,
        const PlayerCommand& command, const PawnSettings& settings, float delta,
        std::span<const physics::BodyId> ignored)
    {
        const bool canLean = !isRunningCommand(command, motion) && !isSliding(motion) && !isMantling(motion);
        const float target = canLean ? math::clamp(static_cast<float>(command.lean), -1.0f, 1.0f) : 0.0f;
        const float step = settings.leanSeconds > 0.0f ? delta / settings.leanSeconds : 1.0f;
        motion.lean = target > motion.lean ? std::min(motion.lean + step, target) : std::max(motion.lean - step, target);

        if (motion.lean == 0.0f)
        {
            return;
        }

        const float side = motion.lean > 0.0f ? 1.0f : -1.0f;
        const float allowed = allowedLean(motion, controller, physics, side, command.yaw, settings, ignored);
        motion.lean = side * std::min(std::abs(motion.lean), allowed);
    }

    float leanOffset(const PawnMotion& motion, const PawnSettings& settings)
    {
        return motion.lean * settings.leanDistance;
    }
} //namespace game
