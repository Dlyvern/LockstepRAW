#include "Player/Pawns.hpp"

#include "Archetypes.hpp"
#include "Player/PawnLean.hpp"

#include <Lockstep/Net/Replication.hpp>
#include <Lockstep/Physics/PhysicsWorld.hpp>

#include <algorithm>
#include <cmath>
#include <optional>

namespace game
{
    namespace
    {
        namespace physics = lockstep::physics;
        namespace world = lockstep::world;

        constexpr math::Vec3 k_upAxis{0.0f, 1.0f, 0.0f};
        constexpr float k_minCrouchRange{0.01f};
        constexpr math::Vec3 k_backAxis{0.0f, 0.0f, 1.0f};
        constexpr float k_maxLeanSine{0.9f};

        math::Quat yawRotation(float yaw)
        {
            return math::angleAxis(yaw, k_upAxis);
        }

        math::Vec3 pawnScale(float height)
        {
            return math::Vec3{2.0f * k_pawnRadius, height, 2.0f * k_pawnRadius};
        }
    } //namespace

    lockstep::Status spawnPawn(Pawn& pawn, world::Entities& entities, lockstep::net::Replication& replication,
        physics::PhysicsWorld& physics, world::EntityOwner owner, const math::Vec3& footPosition,
        const PawnSettings& settings)
    {
        auto entity = entities.create({
            .transform = {.position = footPosition + k_upAxis * (0.5f * k_pawnStandHeight),
                .rotation = yawRotation(pawn.command.yaw), .scale = pawnScale(k_pawnStandHeight)},
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

    void placePawnEntity(world::Entity& entity, const math::Vec3& footPosition, float yaw, float height,
        float leanOffset)
    {
        const float eyeHeight = std::max(height - k_pawnEyeBelowTop, k_minCrouchRange);
        const float roll = std::asin(math::clamp(leanOffset / eyeHeight, -k_maxLeanSine, k_maxLeanSine));
        math::Transform& transform = entity.transform();
        transform.position = footPosition + k_upAxis * (0.5f * height);
        transform.rotation = yawRotation(yaw) * math::angleAxis(-roll, k_backAxis);
        transform.scale = pawnScale(height);
    }

    void movePawn(Pawn& pawn, world::Entity& entity, physics::PhysicsWorld& physics, const PawnSettings& settings,
        float delta, std::span<const physics::BodyId> ignored)
    {
        const std::optional<math::Vec3> foot = stepPawn(pawn.motion, pawn.controller, physics, pawn.command, settings,
            delta, ignored);
        placePawnEntity(entity, foot.value_or(pawnFootPosition(entity.transform())), pawn.command.yaw,
            stanceHeight(pawn.motion.stance, settings), leanOffset(pawn.motion, settings));
    }

    math::Vec3 pawnFootPosition(const math::Transform& transform)
    {
        return transform.position - k_upAxis * (0.5f * transform.scale.y);
    }

    physics::ControllerDescription pawnController(const math::Vec3& footPosition)
    {
        return physics::ControllerDescription{
            .footPosition = footPosition, .radius = k_pawnRadius, .height = k_pawnStandHeight};
    }

    math::Vec3 eyePosition(const math::Transform& transform)
    {
        return pawnFootPosition(transform) + (transform.rotation * k_upAxis) * (transform.scale.y - k_pawnEyeBelowTop);
    }

    float pawnLeanRoll(const math::Transform& transform)
    {
        const math::Vec3 up = transform.rotation * k_upAxis;
        const math::Vec3 forward = transform.rotation * math::Vec3{0.0f, 0.0f, -1.0f};
        const float horizontal = std::max(std::sqrt(forward.x * forward.x + forward.z * forward.z), k_minCrouchRange);
        const math::Vec3 right{-forward.z / horizontal, 0.0f, forward.x / horizontal};

        return std::atan2(math::dot(up, right), up.y);
    }

    float pawnCrouchAmount(const math::Transform& transform, const PawnSettings& settings)
    {
        const float range = std::max(k_pawnStandHeight - settings.crouchHeight, k_minCrouchRange);

        return std::max((k_pawnStandHeight - transform.scale.y) / range, 0.0f);
    }

    math::Vec3 lookDirection(float yaw, float pitch)
    {
        const float horizontal = std::cos(pitch);
        return math::Vec3{-std::sin(yaw) * horizontal, std::sin(pitch), -std::cos(yaw) * horizontal};
    }
} //namespace game
