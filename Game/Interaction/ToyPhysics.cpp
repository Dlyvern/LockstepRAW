#include "Interaction/ToyPhysics.hpp"

#include <Lockstep/Physics/PhysicsWorld.hpp>

#include <algorithm>
#include <cmath>
#include <iostream>

namespace game
{
    namespace
    {
        namespace physics = lockstep::physics;
        namespace world = lockstep::world;

        constexpr float k_ballDensity{90.0f};
        constexpr float k_ballFriction{0.6f};
        constexpr float k_ballRestitution{0.65f};
        constexpr float k_kickReach{0.15f};
        constexpr float k_kickMinSpeed{0.8f};
        constexpr float k_kickSpeedScale{0.9f};
        constexpr float k_kickBaseSpeed{1.0f};
        constexpr float k_kickLift{1.5f};
        constexpr float k_kickCooldown{0.35f};
        constexpr float k_kickHeight{1.0f};
        constexpr float k_rollDamping{1.0f};
        constexpr float k_settleSpeed{0.25f};
        constexpr float k_stillSpeed{0.001f};
        constexpr float k_groundGap{0.03f};
        constexpr float k_groundedSpeed{0.2f};
        constexpr float k_fallLimit{20.0f};

        math::Vec3 horizontal(const math::Vec3& vector)
        {
            return math::Vec3{vector.x, 0.0f, vector.z};
        }

        void kick(Toy& toy, physics::PhysicsWorld& physics, const math::Vec3& centre,
            const std::array<std::optional<ToyPlayer>, k_playerCount>& players)
        {
            for (const std::optional<ToyPlayer>& player : players)
            {
                if (!player || toy.kickCooldown > 0.0f)
                {
                    continue;
                }

                const math::Vec3 away = horizontal(centre - player->foot);
                const float distance = math::length(away);
                const float height = centre.y - player->foot.y;
                const math::Vec3 walking = horizontal(player->velocity);
                const float speed = math::length(walking);

                if (distance > k_toyPawnRadius + toy.radius + k_kickReach || distance <= 0.0f || height > k_kickHeight
                    || speed < k_kickMinSpeed || math::dot(walking, away) <= 0.0f)
                {
                    continue;
                }

                const math::Vec3 push = away * (1.0f / distance) * (speed * k_kickSpeedScale + k_kickBaseSpeed);
                physics.setVelocity(toy.body, push + math::Vec3{0.0f, k_kickLift, 0.0f});
                toy.kickCooldown = k_kickCooldown;
                std::clog << "Game: " << toy.name << " kicked at " << math::length(push) << " m/s\n";
            }
        }

        bool isOnGround(const Toy& toy, const physics::PhysicsWorld& physics, const math::Vec3& centre)
        {
            return physics.raycast(centre, math::Vec3{0.0f, -1.0f, 0.0f}, toy.radius + k_groundGap,
                physics::RaycastIgnore{.body = toy.body}).has_value();
        }

        void rollToStop(const Toy& toy, physics::PhysicsWorld& physics, const math::Vec3& centre, float worldDelta)
        {
            const std::optional<math::Vec3> linear = physics.linearVelocity(toy.body);
            const std::optional<math::Vec3> angular = physics.angularVelocity(toy.body);

            if (!linear || !angular || std::abs(linear->y) > k_groundedSpeed)
            {
                return;
            }

            const float speed = math::length(*linear);

            if (speed < k_settleSpeed)
            {
                if (isOnGround(toy, physics, centre) && (speed > k_stillSpeed || math::length(*angular) > k_stillSpeed))
                {
                    physics.setVelocity(toy.body, math::Vec3{0.0f, 0.0f, 0.0f});
                }

                return;
            }

            const float keep = std::max(1.0f - k_rollDamping * worldDelta, 0.0f);
            physics.setVelocity(toy.body, math::Vec3{linear->x * keep, linear->y, linear->z * keep}, *angular * keep);
        }
    } //namespace

    lockstep::Result<physics::BodyId> createToyBody(physics::PhysicsWorld& physics, const Toy& toy,
        const math::Vec3& position)
    {
        return physics.createBody(physics::BodyDescription{.shape = physics::SphereShape{.radius = toy.radius},
            .type = physics::BodyType::Dynamic,
            .transform = math::Transform{.position = position},
            .density = k_ballDensity,
            .friction = k_ballFriction,
            .restitution = k_ballRestitution});
    }

    void updateFreeToy(Toy& toy, world::Entity& entity, physics::PhysicsWorld& physics,
        const std::array<std::optional<ToyPlayer>, k_playerCount>& players, float worldDelta)
    {
        std::optional<math::Transform> transform = physics.transform(toy.body);

        if (!transform)
        {
            return;
        }

        if (transform->position.y < toy.rest.y - k_fallLimit)
        {
            transform = math::Transform{.position = toy.rest};
            physics.setTransform(toy.body, *transform);
            physics.setVelocity(toy.body, math::Vec3{0.0f, 0.0f, 0.0f});
            entity.teleport(*transform);
            std::clog << "Game: toy " << toy.name << " fell out of the level, back to its spot\n";
            return;
        }

        toy.kickCooldown = std::max(toy.kickCooldown - worldDelta, 0.0f);
        kick(toy, physics, transform->position, players);
        rollToStop(toy, physics, transform->position, worldDelta);
        entity.transform() = *transform;
    }
} //namespace game
