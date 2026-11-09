#include "Weapons/Hitscan.hpp"

#include <Lockstep/Physics/PhysicsWorld.hpp>

#include <optional>

namespace game
{
    namespace physics = lockstep::physics;

    ShotResult traceShot(const physics::PhysicsWorld& physics, const math::Vec3& origin, const math::Vec3& direction,
        float range, physics::ControllerId shooter, physics::ControllerId enemy)
    {
        const std::optional<physics::RaycastHit> hit = physics.raycast(origin, direction, range,
            physics::RaycastIgnore{.controller = shooter});

        if (!hit)
        {
            return ShotResult{};
        }

        if (hit->controller.isNull())
        {
            return ShotResult{.outcome = ShotOutcome::Level, .distance = hit->distance, .body = hit->body};
        }

        const bool isEnemy = !enemy.isNull() && hit->controller == enemy;
        return ShotResult{.outcome = isEnemy ? ShotOutcome::Enemy : ShotOutcome::Player,
            .distance = hit->distance,
            .controller = hit->controller};
    }

    const char* toString(ShotOutcome outcome) noexcept
    {
        switch (outcome)
        {
            case ShotOutcome::Miss:
                return "miss";
            case ShotOutcome::Enemy:
                return "enemy";
            case ShotOutcome::Player:
                return "player";
            case ShotOutcome::Level:
                return "level";
        }

        return "unknown";
    }
} //namespace game
