#include "Player/Pawns.hpp"

#include "Archetypes.hpp"

#include <Lockstep/Net/Replication.hpp>
#include <Lockstep/Physics/PhysicsWorld.hpp>

#include <cmath>
#include <optional>

namespace game
{
    namespace
    {
        namespace physics = lockstep::physics;
        namespace world = lockstep::world;

        constexpr math::Vec3 k_upAxis{0.0f, 1.0f, 0.0f};
        constexpr math::Vec3 k_footToCentre{0.0f, k_pawnHalfExtents.y, 0.0f};

        math::Quat yawRotation(float yaw)
        {
            return math::angleAxis(yaw, k_upAxis);
        }

        math::Vec3 walkVelocity(const PlayerCommand& command, float speed)
        {
            math::Vec2 move = command.move;
            const float moveLength = math::length(move);

            if (moveLength > 1.0f)
            {
                move = move * (1.0f / moveLength);
            }

            const math::Vec3 forward{-std::sin(command.yaw), 0.0f, -std::cos(command.yaw)};
            const math::Vec3 right{std::cos(command.yaw), 0.0f, -std::sin(command.yaw)};

            return (forward * move.y + right * move.x) * speed;
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

        const math::Vec3 velocity = walkVelocity(command, settings.moveSpeed) + k_upAxis * motion.verticalSpeed;
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
