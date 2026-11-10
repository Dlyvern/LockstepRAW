#include "Player/PawnMantle.hpp"

#include "Player/PawnRays.hpp"

#include <Lockstep/Physics/PhysicsWorld.hpp>

#include <algorithm>
#include <cmath>

namespace game
{
    namespace
    {
        namespace physics = lockstep::physics;

        constexpr math::Vec3 k_upAxis{0.0f, 1.0f, 0.0f};
        constexpr float k_wallProbeHeight{0.4f};
        constexpr float k_topInset{0.15f};
        constexpr float k_rayMargin{0.05f};
        constexpr float k_ledgeLift{0.02f};
        constexpr float k_minWalkableNormal{0.7f};
        constexpr float k_minForwardMove{0.5f};
        constexpr float k_riseTolerance{0.005f};
        constexpr float k_minRiseFraction{0.25f};

        struct Ledge
        {
            float top{0.0f};
        };

        bool isClear(const physics::PhysicsWorld& physics, const math::Vec3& origin, const math::Vec3& direction,
            float distance, physics::ControllerId controller, std::span<const physics::BodyId> ignored)
        {
            return !sharedRaycast(physics, origin, direction, distance, controller, ignored).has_value();
        }

        std::optional<Ledge> findLedge(const math::Vec3& foot, const math::Vec3& forward, float height,
            physics::ControllerId controller, const physics::PhysicsWorld& physics, const PawnSettings& settings,
            std::span<const physics::BodyId> ignored)
        {
            const float reach = k_pawnRadius + settings.mantleReach;
            const auto wall = sharedRaycast(physics, foot + k_upAxis * k_wallProbeHeight, forward, reach, controller,
                ignored);

            if (!wall)
            {
                return std::nullopt;
            }

            const float inset = wall->distance + k_topInset;
            const float probeHeight = settings.mantleMaxHeight + k_rayMargin;
            const auto top = sharedRaycast(physics, foot + k_upAxis * probeHeight + forward * inset, -k_upAxis,
                probeHeight - settings.mantleMinHeight, controller, ignored);

            if (!top || top->normal.y < k_minWalkableNormal)
            {
                return std::nullopt;
            }

            const float rise = top->point.y - foot.y;
            const float across = inset + 2.0f * k_pawnRadius;
            const bool hasRoom = isClear(physics, foot + k_upAxis * (height - k_rayMargin), k_upAxis,
                    rise + k_rayMargin, controller, ignored)
                && isClear(physics, top->point + k_upAxis * k_rayMargin, k_upAxis, height, controller, ignored)
                && isClear(physics, foot + k_upAxis * (rise + k_rayMargin), forward, across, controller, ignored)
                && isClear(physics, foot + k_upAxis * (rise + 0.5f * height), forward, across, controller, ignored);

            if (!hasRoom)
            {
                return std::nullopt;
            }

            return Ledge{.top = top->point.y};
        }

        void endMantle(PawnMotion& motion, const PawnSettings& settings)
        {
            motion.mantleTicks = 0;
            const float speed = math::length(motion.planarVelocity);

            if (speed > settings.walkSpeed)
            {
                motion.planarVelocity = motion.planarVelocity * (settings.walkSpeed / speed);
            }
        }
    } //namespace

    bool tryStartMantle(PawnMotion& motion, physics::ControllerId controller, const physics::PhysicsWorld& physics,
        const PlayerCommand& command, const PawnSettings& settings, float delta,
        std::span<const physics::BodyId> ignored)
    {
        if (motion.jumpBufferTicks == 0 || command.move.y < k_minForwardMove || settings.mantleMaxHeight <= 0.0f)
        {
            return false;
        }

        const std::optional<math::Vec3> foot = physics.footPosition(controller);

        if (!foot)
        {
            return false;
        }

        const math::Vec3 forward{-std::sin(command.yaw), 0.0f, -std::cos(command.yaw)};
        const std::optional<Ledge> ledge = findLedge(*foot, forward, stanceHeight(motion.stance, settings), controller,
            physics, settings, ignored);

        if (!ledge)
        {
            return false;
        }

        const float forwardSeconds = std::max(settings.mantleForwardSeconds, delta);
        motion.mantleTicks = std::max(secondsToTicks(forwardSeconds, delta), uint8_t{1});
        motion.mantleHeight = ledge->top + k_ledgeLift;
        motion.planarVelocity = forward * (settings.mantleForwardDistance / forwardSeconds);
        motion.verticalSpeed = 0.0f;
        motion.isGrounded = false;
        motion.jumpBufferTicks = 0;
        motion.coyoteTicks = 0;
        motion.slideSeconds = 0.0f;

        return true;
    }

    std::optional<physics::ControllerMove> stepMantle(PawnMotion& motion, physics::ControllerId controller,
        physics::PhysicsWorld& physics, const PawnSettings& settings, float delta)
    {
        const std::optional<math::Vec3> start = physics.footPosition(controller);

        if (!start)
        {
            return std::nullopt;
        }

        motion.verticalSpeed = 0.0f;
        const float below = motion.mantleHeight - start->y;

        if (below > k_riseTolerance)
        {
            const float rise = std::min(settings.mantleRiseSpeed * delta, below);
            const std::optional<physics::ControllerMove> moved = physics.move(controller, k_upAxis * rise, delta);
            const std::optional<math::Vec3> end = physics.footPosition(controller);

            if (!moved || !end || moved->hitCeiling || end->y - start->y < rise * k_minRiseFraction)
            {
                endMantle(motion, settings);
            }

            motion.isGrounded = false;
            return moved;
        }

        const std::optional<physics::ControllerMove> moved = physics.move(controller, motion.planarVelocity * delta,
            delta);
        --motion.mantleTicks;

        if (motion.mantleTicks == 0)
        {
            endMantle(motion, settings);
        }

        motion.isGrounded = moved && moved->grounded && motion.mantleTicks == 0;
        return moved;
    }
} //namespace game
