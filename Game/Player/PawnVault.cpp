#include "Player/PawnVault.hpp"

#include "Player/PawnRays.hpp"

#include <Lockstep/Physics/PhysicsWorld.hpp>

#include <algorithm>
#include <array>
#include <cmath>

namespace game
{
    namespace
    {
        namespace physics = lockstep::physics;

        constexpr math::Vec3 k_upAxis{0.0f, 1.0f, 0.0f};
        constexpr std::array<float, 3> k_wallProbeHeights{0.3f, 0.55f, 0.8f};
        constexpr float k_sampleStep{0.06f};
        constexpr float k_firstSample{0.03f};
        constexpr float k_floorIgnoreHeight{0.15f};
        constexpr float k_edgeDrop{0.3f};
        constexpr float k_rayMargin{0.05f};
        constexpr float k_minWalkableNormal{0.7f};
        constexpr float k_minForwardMove{0.5f};
        constexpr float k_riseTolerance{0.005f};
        constexpr float k_minRiseFraction{0.25f};
        constexpr float k_minCrossFraction{0.3f};
        constexpr float k_minSpeed{0.01f};

        struct Obstacle
        {
            float top{0.0f};
            float gap{0.0f};
            float landDistance{0.0f};
        };

        bool isClear(const physics::PhysicsWorld& physics, const math::Vec3& origin, const math::Vec3& direction,
            float distance, physics::ControllerId controller, std::span<const physics::BodyId> ignored)
        {
            return !sharedRaycast(physics, origin, direction, distance, controller, ignored).has_value();
        }

        std::optional<float> nearestWall(const math::Vec3& foot, const math::Vec3& forward, float reach,
            physics::ControllerId controller, const physics::PhysicsWorld& physics,
            std::span<const physics::BodyId> ignored)
        {
            std::optional<float> nearest;

            for (const float height : k_wallProbeHeights)
            {
                const auto hit = sharedRaycast(physics, foot + k_upAxis * height, forward, reach, controller, ignored);

                if (hit && (!nearest || hit->distance < *nearest))
                {
                    nearest = hit->distance;
                }
            }

            return nearest;
        }

        struct Profile
        {
            float top{0.0f};
            float farEdge{0.0f};
        };

        std::optional<Profile> measureProfile(const math::Vec3& foot, const math::Vec3& forward, float wall,
            physics::ControllerId controller, const physics::PhysicsWorld& physics, const VaultSettings& vault,
            std::span<const physics::BodyId> ignored)
        {
            const float probeHeight = vault.maxHeight + k_rayMargin * 3.0f;
            const float probeLength = probeHeight - k_floorIgnoreHeight;
            std::optional<float> top;

            for (float inset = wall + k_firstSample; inset <= wall + vault.maxThickness + k_sampleStep;
                inset += k_sampleStep)
            {
                const auto hit = sharedRaycast(physics, foot + k_upAxis * probeHeight + forward * inset, -k_upAxis,
                    probeLength, controller, ignored);
                const float height = hit ? hit->point.y - foot.y : 0.0f;

                if (!top || height > *top)
                {
                    top = height;
                    continue;
                }

                if (height < *top - k_edgeDrop)
                {
                    return Profile{.top = *top, .farEdge = inset};
                }
            }

            return std::nullopt;
        }

        std::optional<Obstacle> findObstacle(const math::Vec3& foot, const math::Vec3& forward, float height,
            float speed, physics::ControllerId controller, const physics::PhysicsWorld& physics,
            const PawnSettings& settings, std::span<const physics::BodyId> ignored)
        {
            const VaultSettings& vault = settings.vault;
            const float reach = k_pawnRadius + vault.reach + vault.reachPerSpeed * speed;
            const std::optional<float> wall = nearestWall(foot, forward, reach, controller, physics, ignored);

            if (!wall)
            {
                return std::nullopt;
            }

            const std::optional<Profile> profile = measureProfile(foot, forward, *wall, controller, physics, vault,
                ignored);

            if (!profile || profile->top < vault.minHeight || profile->top > vault.maxHeight)
            {
                return std::nullopt;
            }

            const float landDistance = profile->farEdge + k_pawnRadius + vault.landMargin
                + vault.sprintDistance * std::max(speed - settings.walkSpeed, 0.0f);
            const float pass = profile->top + vault.clearance;
            const math::Vec3 landing = foot + forward * landDistance;
            const float floorProbe = pass + k_rayMargin;
            const auto floor = sharedRaycast(physics, landing + k_upAxis * floorProbe, -k_upAxis,
                floorProbe + vault.maxDrop, controller, ignored);

            if (!floor || floor->normal.y < k_minWalkableNormal || floor->point.y > foot.y + profile->top - k_edgeDrop)
            {
                return std::nullopt;
            }

            const bool hasRoom = isClear(physics, foot + k_upAxis * (height - k_rayMargin), k_upAxis,
                    pass + k_rayMargin, controller, ignored)
                && isClear(physics, foot + k_upAxis * (pass + k_rayMargin), forward, landDistance, controller, ignored)
                && isClear(physics, foot + k_upAxis * (pass + 0.5f * height), forward, landDistance, controller,
                    ignored)
                && isClear(physics, foot + k_upAxis * (pass + height - k_rayMargin), forward, landDistance, controller,
                    ignored)
                && isClear(physics, floor->point + k_upAxis * k_rayMargin, k_upAxis, height, controller, ignored);

            if (!hasRoom)
            {
                return std::nullopt;
            }

            return Obstacle{.top = foot.y + profile->top,
                .gap = std::max(*wall - k_pawnRadius - k_rayMargin, 0.0f),
                .landDistance = landDistance};
        }

        float riseSpeed(const VaultSettings& vault, const PawnSettings& settings, float crossSpeed)
        {
            return vault.riseSpeed + vault.riseSpeedPerSpeed * std::max(crossSpeed - settings.walkSpeed, 0.0f);
        }

        void endVault(PawnMotion& motion, const VaultSettings& vault, bool isCleared)
        {
            motion.mantleTicks = 0;
            motion.isVaulting = false;
            motion.isGrounded = false;
            motion.planarVelocity = motion.planarVelocity * (isCleared ? vault.exitScale : 0.0f);
            motion.verticalSpeed = isCleared ? -vault.exitDropSpeed : 0.0f;
        }
    } //namespace

    bool tryStartVault(PawnMotion& motion, physics::ControllerId controller, const physics::PhysicsWorld& physics,
        const PlayerCommand& command, const PawnSettings& settings, float delta,
        std::span<const physics::BodyId> ignored)
    {
        const VaultSettings& vault = settings.vault;
        const bool isOnGround = motion.isGrounded || motion.coyoteTicks > 0 || isSliding(motion);

        if (motion.jumpBufferTicks == 0 || command.move.y < k_minForwardMove || !isOnGround
            || vault.maxHeight <= vault.minHeight)
        {
            return false;
        }

        const std::optional<math::Vec3> foot = physics.footPosition(controller);

        if (!foot)
        {
            return false;
        }

        const math::Vec3 forward{-std::sin(command.yaw), 0.0f, -std::cos(command.yaw)};
        const float speed = math::length(motion.planarVelocity);
        const std::optional<Obstacle> obstacle = findObstacle(*foot, forward, stanceHeight(motion.stance, settings),
            speed, controller, physics, settings, ignored);

        if (!obstacle)
        {
            return false;
        }

        const float crossSpeed = math::clamp(speed, vault.minSpeed, std::max(vault.maxSpeed, vault.minSpeed));
        const float rise = obstacle->top + vault.clearance - foot->y;
        const float riseStep = std::max(riseSpeed(vault, settings, crossSpeed) * delta, k_minSpeed);
        const float riseTicks = std::ceil(rise / riseStep);
        const float riseForward = std::min(obstacle->gap, crossSpeed * delta * riseTicks);
        const float crossTicks = std::ceil(std::max(obstacle->landDistance - riseForward, 0.0f)
            / std::max(crossSpeed * delta, k_minSpeed));
        motion.mantleTicks = static_cast<uint8_t>(math::clamp(crossTicks, 1.0f, 255.0f));
        motion.mantleHeight = obstacle->top + vault.clearance;
        motion.isVaulting = true;
        motion.planarVelocity = forward * crossSpeed;
        motion.verticalSpeed = 0.0f;
        motion.isGrounded = false;
        motion.jumpBufferTicks = 0;
        motion.coyoteTicks = 0;
        motion.slideSeconds = 0.0f;

        return true;
    }

    std::optional<physics::ControllerMove> stepVault(PawnMotion& motion, physics::ControllerId controller,
        physics::PhysicsWorld& physics, const PawnSettings& settings, float delta)
    {
        const VaultSettings& vault = settings.vault;
        const std::optional<math::Vec3> start = physics.footPosition(controller);

        if (!start)
        {
            return std::nullopt;
        }

        motion.sprintRecoverTicks = std::max(motion.sprintRecoverTicks,
            secondsToTicks(settings.sprintRecoverSeconds, delta));
        motion.verticalSpeed = 0.0f;
        const float below = motion.mantleHeight - start->y;

        if (below > k_riseTolerance)
        {
            const float speed = riseSpeed(vault, settings, math::length(motion.planarVelocity));
            const float rise = std::min(speed * delta, below);
            const std::optional<physics::ControllerMove> moved = physics.move(controller,
                k_upAxis * rise + motion.planarVelocity * delta, delta);
            const std::optional<math::Vec3> end = physics.footPosition(controller);

            if (!moved || !end || moved->hitCeiling || end->y - start->y < rise * k_minRiseFraction)
            {
                endVault(motion, vault, false);
            }

            motion.isGrounded = false;
            return moved;
        }

        const math::Vec3 wanted = motion.planarVelocity * delta;
        const std::optional<physics::ControllerMove> moved = physics.move(controller, wanted, delta);
        const std::optional<math::Vec3> end = physics.footPosition(controller);
        --motion.mantleTicks;

        if (!moved || !end)
        {
            endVault(motion, vault, false);
            return moved;
        }

        math::Vec3 travelled = *end - *start;
        travelled.y = 0.0f;
        const bool isBlocked = math::length(travelled) < math::length(wanted) * k_minCrossFraction;

        if (motion.mantleTicks == 0 || isBlocked)
        {
            endVault(motion, vault, !isBlocked);
        }

        motion.isGrounded = false;
        return moved;
    }
} //namespace game
