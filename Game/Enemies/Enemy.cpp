#include "Enemies/Enemy.hpp"

#include "Archetypes.hpp"

#include <Lockstep/Net/Replication.hpp>
#include <Lockstep/Physics/PhysicsWorld.hpp>

#include <algorithm>
#include <cmath>

namespace game
{
    namespace
    {
        namespace physics = lockstep::physics;
        namespace world = lockstep::world;

        constexpr math::Vec3 k_upAxis{0.0f, 1.0f, 0.0f};
        constexpr math::Vec3 k_footToCentre{0.0f, k_enemyHalfExtents.y, 0.0f};
        constexpr float k_groundStick{2.0f};

        math::Vec3 horizontal(const math::Vec3& vector)
        {
            return math::Vec3{vector.x, 0.0f, vector.z};
        }

        const EnemyTarget* nearestTarget(const math::Vec3& position, std::span<const EnemyTarget> targets)
        {
            const EnemyTarget* nearest = nullptr;
            float nearestDistance = 0.0f;

            for (const EnemyTarget& target : targets)
            {
                const float distance = math::lengthSquared(horizontal(target.position - position));

                if (nearest == nullptr || distance < nearestDistance)
                {
                    nearest = &target;
                    nearestDistance = distance;
                }
            }

            return nearest;
        }

        math::Vec3 walkDisplacement(math::Transform& transform, const math::Vec3& target, const EnemySettings& settings,
            float delta)
        {
            const math::Vec3 offset = horizontal(target - transform.position);
            const float distance = math::length(offset);

            if (distance <= 0.0f)
            {
                return {};
            }

            const math::Vec3 direction = offset * (1.0f / distance);
            transform.rotation = math::angleAxis(std::atan2(-direction.x, -direction.z), k_upAxis);

            const float step = std::min(settings.walkSpeed * delta, std::max(distance - settings.stopDistance, 0.0f));
            return direction * step;
        }

        void syncEntity(world::Entity& entity, const physics::PhysicsWorld& physics, physics::ControllerId controller)
        {
            if (const std::optional<math::Vec3> foot = physics.footPosition(controller))
            {
                entity.transform().position = *foot + k_footToCentre;
            }
        }
    } //namespace

    lockstep::Status spawnEnemy(Enemy& enemy, world::Entities& entities, lockstep::net::Replication& replication,
        physics::PhysicsWorld& physics, const EnemySettings& settings)
    {
        auto entity = entities.create({
            .transform = {.position = settings.spawnPoint + k_footToCentre, .scale = k_enemyHalfExtents * 2.0f},
            .domain = world::TimeDomain::World,
            .owner = world::EntityOwner::Host});

        if (!entity)
        {
            return entity.error();
        }

        if (auto tagged = replication.setArchetype(entity.value(), static_cast<uint16_t>(Archetype::Enemy)); !tagged)
        {
            entities.destroy(entity.value());
            return tagged.error();
        }

        auto controller = physics.createController({
            .footPosition = settings.spawnPoint,
            .radius = k_enemyHalfExtents.x,
            .height = k_enemyHalfExtents.y * 2.0f});

        if (!controller)
        {
            entities.destroy(entity.value());
            return controller.error();
        }

        enemy.entity = entity.value();
        enemy.controller = controller.value();
        enemy.phase = EnemyPhase::Alive;
        enemy.health = settings.maxHealth;
        enemy.respawnTimer = 0.0f;
        enemy.attackCooldown = settings.attackCooldown;
        enemy.verticalSpeed = 0.0f;
        enemy.isGrounded = false;

        return {};
    }

    lockstep::Status updateEnemy(Enemy& enemy, world::Entities& entities, lockstep::net::Replication& replication,
        physics::PhysicsWorld& physics, const EnemySettings& settings, std::span<const EnemyTarget> targets,
        float delta)
    {
        if (enemy.phase == EnemyPhase::Dead)
        {
            enemy.respawnTimer = std::max(enemy.respawnTimer - delta, 0.0f);

            if (enemy.respawnTimer > 0.0f)
            {
                return {};
            }

            return spawnEnemy(enemy, entities, replication, physics, settings);
        }

        world::Entity* entity = entities.find(enemy.entity);

        if (entity == nullptr || delta <= 0.0f)
        {
            return {};
        }

        math::Vec3 displacement{};

        if (const EnemyTarget* target = nearestTarget(entity->transform().position, targets))
        {
            displacement = walkDisplacement(entity->transform(), target->position, settings, delta);
        }

        enemy.verticalSpeed = enemy.isGrounded ? -k_groundStick : enemy.verticalSpeed - settings.gravity * delta;
        displacement = displacement + k_upAxis * (enemy.verticalSpeed * delta);

        if (const std::optional<physics::ControllerMove> moved = physics.move(enemy.controller, displacement, delta))
        {
            enemy.isGrounded = moved->grounded;
        }

        syncEntity(*entity, physics, enemy.controller);

        return {};
    }

    std::optional<std::size_t> enemyAttack(Enemy& enemy, const world::Entities& entities,
        const EnemySettings& settings, std::span<const EnemyTarget> targets, float delta)
    {
        const world::Entity* entity = entities.find(enemy.entity);

        if (entity == nullptr)
        {
            return std::nullopt;
        }

        enemy.attackCooldown = std::max(enemy.attackCooldown - delta, 0.0f);

        if (enemy.attackCooldown > 0.0f)
        {
            return std::nullopt;
        }

        const math::Vec3& position = entity->transform().position;
        const EnemyTarget* target = nearestTarget(position, targets);

        if (target == nullptr)
        {
            return std::nullopt;
        }

        const math::Vec3 offset = target->position - position;

        if (math::length(horizontal(offset)) > settings.attackRange || std::abs(offset.y) > settings.attackHeight)
        {
            return std::nullopt;
        }

        enemy.attackCooldown = settings.attackCooldown;
        return target->pawnIndex;
    }

    bool damageEnemy(Enemy& enemy, world::Entities& entities, physics::PhysicsWorld& physics,
        const EnemySettings& settings, const math::Vec3& direction, float delta)
    {
        enemy.health -= 1;

        if (enemy.health > 0)
        {
            if (world::Entity* entity = entities.find(enemy.entity))
            {
                physics.move(enemy.controller, horizontal(direction) * settings.knockback, delta);
                syncEntity(*entity, physics, enemy.controller);
            }

            return false;
        }

        entities.destroy(enemy.entity);
        physics.destroyController(enemy.controller);
        enemy.entity = {};
        enemy.controller = {};
        enemy.phase = EnemyPhase::Dead;
        enemy.health = 0;
        enemy.respawnTimer = settings.respawnDelay;
        ++enemy.deaths;

        return true;
    }

    const char* toString(EnemyPhase phase) noexcept
    {
        switch (phase)
        {
            case EnemyPhase::Alive:
                return "alive";
            case EnemyPhase::Dead:
                return "dead";
        }

        return "unknown";
    }
} //namespace game
