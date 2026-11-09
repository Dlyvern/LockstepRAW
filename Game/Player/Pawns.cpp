#include "Player/Pawns.hpp"

#include "Archetypes.hpp"

#include <Lockstep/Net/Replication.hpp>
#include <Lockstep/Physics/PhysicsWorld.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>

namespace game
{
    namespace
    {
        namespace physics = lockstep::physics;
        namespace world = lockstep::world;

        constexpr math::Vec3 k_upAxis{0.0f, 1.0f, 0.0f};
        constexpr math::Vec3 k_footToCentre{0.0f, k_pawnHalfExtents.y, 0.0f};
        constexpr float k_minSpeed{0.01f};
        constexpr float k_minMoveLength{0.001f};

        math::Quat yawRotation(float yaw)
        {
            return math::angleAxis(yaw, k_upAxis);
        }

        float ellipseSpeed(const math::Vec2& direction, float sideSpeed, float forwardSpeed)
        {
            const float side = direction.x / std::max(sideSpeed, k_minSpeed);
            const float forward = direction.y / std::max(forwardSpeed, k_minSpeed);

            return 1.0f / std::sqrt(side * side + forward * forward);
        }

        math::Vec3 wantedVelocity(const PlayerCommand& command, const PawnSettings& settings)
        {
            math::Vec2 move = command.move;
            const float moveLength = math::length(move);

            if (moveLength < k_minMoveLength)
            {
                return math::Vec3{0.0f, 0.0f, 0.0f};
            }

            const bool isRunning = command.sprint && !command.aim && move.y > 0.0f;
            const float speedCap = command.aim ? settings.aimSpeed : std::numeric_limits<float>::max();
            const float runSpeed = command.isWeaponUp ? std::min(settings.runSpeed, settings.weaponUpRunSpeed)
                                                      : settings.runSpeed;
            const float forwardSpeed = std::min(speedCap, move.y < 0.0f ? settings.backSpeed
                                                     : (isRunning ? runSpeed : settings.walkSpeed));
            const float sideSpeed = std::min(speedCap, isRunning ? settings.sprintStrafeSpeed : settings.strafeSpeed);
            const math::Vec2 direction = move * (1.0f / moveLength);
            const float speed = std::min(moveLength, 1.0f) * ellipseSpeed(direction, sideSpeed, forwardSpeed);

            const math::Vec3 forward{-std::sin(command.yaw), 0.0f, -std::cos(command.yaw)};
            const math::Vec3 right{std::cos(command.yaw), 0.0f, -std::sin(command.yaw)};

            return (forward * direction.y + right * direction.x) * speed;
        }

        math::Vec3 easeVelocity(const math::Vec3& current, const math::Vec3& wanted, const PawnSettings& settings,
            float delta)
        {
            const math::Vec3 difference = wanted - current;
            const float distance = math::length(difference);
            const bool isSpeedingUp = math::length(wanted) > math::length(current);
            const float maxChange = (isSpeedingUp ? settings.acceleration : settings.deceleration) * delta;

            if (distance <= maxChange)
            {
                return wanted;
            }

            return current + difference * (maxChange / distance);
        }
    } //namespace

    lockstep::Status spawnPawn(Pawn& pawn, world::Entities& entities, lockstep::net::Replication& replication,
        physics::PhysicsWorld& physics, world::EntityOwner owner, const math::Vec3& footPosition,
        const PawnSettings& settings)
    {
        auto entity = entities.create({
            .transform = {.position = footPosition + k_footToCentre, .rotation = yawRotation(pawn.command.yaw),
                .scale = k_pawnHalfExtents * 2.0f},
            .domain = world::TimeDomain::Player,
            .owner = owner});

        if (!entity)
        {
            return entity.error();
        }

        if (auto tagged = replication.setArchetype(entity.value(), static_cast<uint16_t>(Archetype::Pawn)); !tagged)
        {
            entities.destroy(entity.value());
            return tagged.error();
        }

        auto controller = physics.createController(pawnController(footPosition));

        if (!controller)
        {
            entities.destroy(entity.value());
            return controller.error();
        }

        pawn.entity = entity.value();
        pawn.controller = controller.value();
        pawn.motion = PawnMotion{};
        ++pawn.life;
        pawn.isJoined = true;
        pawn.health = settings.maxHealth;
        pawn.respawnTimer = 0.0f;

        return {};
    }

    void despawnPawn(Pawn& pawn, world::Entities& entities, physics::PhysicsWorld& physics)
    {
        entities.destroy(pawn.entity);
        physics.destroyController(pawn.controller);
        pawn.entity = {};
        pawn.controller = {};
    }

    std::optional<math::Vec3> stepPawn(PawnMotion& motion, physics::ControllerId controller,
        physics::PhysicsWorld& physics, const PlayerCommand& command, const PawnSettings& settings, float delta)
    {
        motion.verticalSpeed = motion.isGrounded ? -settings.groundStick : motion.verticalSpeed - settings.gravity * delta;

        motion.planarVelocity = easeVelocity(motion.planarVelocity, wantedVelocity(command, settings), settings, delta);

        const math::Vec3 velocity = motion.planarVelocity + k_upAxis * motion.verticalSpeed;
        const std::optional<physics::ControllerMove> moved = physics.move(controller, velocity * delta, delta);

        if (!moved)
        {
            return std::nullopt;
        }

        motion.isGrounded = moved->grounded;

        if (moved->hitCeiling && motion.verticalSpeed > 0.0f)
        {
            motion.verticalSpeed = 0.0f;
        }

        return physics.footPosition(controller);
    }

    void placePawnEntity(world::Entity& entity, const math::Vec3& footPosition, float yaw)
    {
        math::Transform& transform = entity.transform();
        transform.position = footPosition + k_footToCentre;
        transform.rotation = yawRotation(yaw);
        transform.scale = k_pawnHalfExtents * 2.0f;
    }

    void movePawn(Pawn& pawn, world::Entity& entity, physics::PhysicsWorld& physics, const PawnSettings& settings,
        float delta)
    {
        const std::optional<math::Vec3> foot = stepPawn(pawn.motion, pawn.controller, physics, pawn.command, settings,
            delta);
        placePawnEntity(entity, foot.value_or(pawnFootPosition(entity.transform())), pawn.command.yaw);
    }

    math::Vec3 pawnFootPosition(const math::Transform& transform)
    {
        return transform.position - k_footToCentre;
    }

    physics::ControllerDescription pawnController(const math::Vec3& footPosition)
    {
        return physics::ControllerDescription{
            .footPosition = footPosition, .radius = k_pawnHalfExtents.x, .height = k_pawnHalfExtents.y * 2.0f};
    }

    math::Vec3 eyePosition(const math::Transform& transform)
    {
        return transform.position + math::Vec3{0.0f, k_pawnEyeOffset, 0.0f};
    }

    math::Vec3 lookDirection(float yaw, float pitch)
    {
        const float horizontal = std::cos(pitch);
        return math::Vec3{-std::sin(yaw) * horizontal, std::sin(pitch), -std::cos(yaw) * horizontal};
    }
} //namespace game
