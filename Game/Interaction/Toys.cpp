#include "Interaction/Toys.hpp"

#include "Archetypes.hpp"
#include "Interaction/NodeMotion.hpp"
#include "Interaction/ToyPhysics.hpp"
#include "Level/RoomState.hpp"

#include <Lockstep/Core/Bounds.hpp>
#include <Lockstep/Physics/PhysicsWorld.hpp>
#include <Lockstep/Render/Renderer.hpp>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <iostream>
#include <string_view>

namespace game
{
    namespace
    {
        namespace assets = lockstep::assets;
        namespace physics = lockstep::physics;
        namespace world = lockstep::world;

        constexpr std::string_view k_toyPrefix{"Toy_"};
        constexpr std::string_view k_ballWord{"ball"};
        constexpr std::string_view k_basketballName{"Toy_Basketball"};
        constexpr std::string_view k_courtPrefix{"Court"};
        constexpr float k_basketballRadius{0.12f};
        constexpr float k_basketballDrop{0.3f};
        constexpr float k_minModelRadius{0.001f};
        constexpr float k_holdDistance{0.75f};
        constexpr float k_holdClearance{0.05f};
        constexpr float k_holdDrop{0.25f};
        constexpr float k_boundsTolerance{0.05f};

        bool containsBall(std::string_view name)
        {
            std::string lower(name);
            std::transform(lower.begin(), lower.end(), lower.begin(),
                [](unsigned char character) { return static_cast<char>(std::tolower(character)); });

            return lower.find(k_ballWord) != std::string::npos;
        }

        std::string labelOf(std::string_view name)
        {
            std::string label(name.substr(k_toyPrefix.size()));
            std::transform(label.begin(), label.end(), label.begin(),
                [](unsigned char character) { return static_cast<char>(std::tolower(character)); });

            return label;
        }

        Toy toyFromNode(const assets::Scene& scene, uint32_t node)
        {
            Toy toy{};
            toy.name = scene.nodes[node].name;
            toy.label = labelOf(toy.name);
            math::Aabb bounds{};
            const std::vector<uint32_t> instances = assets::subtreeInstances(scene, node);

            for (const uint32_t instance : instances)
            {
                math::expand(bounds, scene.instances[instance].worldBounds);
            }

            toy.rest = math::center(bounds);
            const math::Vec3 extent = math::size(bounds);
            toy.radius = 0.5f * std::max({extent.x, extent.y, extent.z});

            for (const uint32_t instance : instances)
            {
                math::Transform offset = scene.instances[instance].transform;
                offset.position = offset.position - toy.rest;
                toy.parts.push_back(ToyPart{.instance = instance, .offset = offset});
            }

            return toy;
        }

        std::optional<math::Vec3> courtBallSpot(const assets::Scene& scene)
        {
            math::Aabb court{};
            float floor = court.min.y;
            std::optional<math::Vec3> firstNode;

            for (std::size_t index = 0; index < scene.nodes.size(); ++index)
            {
                if (!scene.nodes[index].name.starts_with(k_courtPrefix))
                {
                    continue;
                }

                firstNode = firstNode.value_or(scene.nodes[index].transform.position);
                math::Aabb piece{};

                for (const uint32_t instance : assets::subtreeInstances(scene, static_cast<uint32_t>(index)))
                {
                    math::expand(piece, scene.instances[instance].worldBounds);
                }

                if (!math::isEmpty(piece))
                {
                    math::expand(court, piece);
                    floor = std::min(floor, piece.max.y);
                }
            }

            if (!math::isEmpty(court))
            {
                const math::Vec3 middle = math::center(court);

                return math::Vec3{middle.x, floor + k_basketballRadius + k_basketballDrop, middle.z};
            }

            if (firstNode)
            {
                return *firstNode + math::Vec3{0.0f, k_basketballRadius + k_basketballDrop, 0.0f};
            }

            return std::nullopt;
        }

        std::optional<Toy> madeBasketball(const assets::Scene& scene, const Toy& model)
        {
            const std::optional<math::Vec3> spot = courtBallSpot(scene);

            if (!spot || model.radius < k_minModelRadius)
            {
                return std::nullopt;
            }

            const float ratio = k_basketballRadius / model.radius;
            Toy toy{};
            toy.name = std::string(k_basketballName);
            toy.label = labelOf(k_basketballName);
            toy.radius = k_basketballRadius;
            toy.rest = *spot;

            for (ToyPart part : model.parts)
            {
                part.offset.position = part.offset.position * ratio;
                part.offset.scale = part.offset.scale * ratio;
                toy.parts.push_back(part);
            }

            return toy;
        }

        math::Transform compose(const math::Transform& frame, const math::Transform& offset)
        {
            return math::Transform{.position = frame.position + frame.rotation * offset.position,
                .rotation = math::normalize(frame.rotation * offset.rotation), .scale = offset.scale};
        }

        math::Vec3 horizontal(const math::Vec3& vector)
        {
            return math::Vec3{vector.x, 0.0f, vector.z};
        }

        std::optional<float> raySphere(const math::Vec3& origin, const math::Vec3& direction, const math::Vec3& centre,
            float radius)
        {
            const math::Vec3 toCentre = centre - origin;
            const float along = math::dot(toCentre, direction);
            const float missSquared = math::lengthSquared(toCentre) - along * along;

            if (missSquared > radius * radius)
            {
                return std::nullopt;
            }

            const float enter = along - std::sqrt(radius * radius - missSquared);

            return enter >= 0.0f ? std::optional<float>(enter) : std::nullopt;
        }

    } //namespace

    std::vector<Toy> findToys(const assets::Scene& scene)
    {
        std::vector<Toy> toys;
        bool hasBasketball = false;

        for (std::size_t index = 0; index < scene.nodes.size(); ++index)
        {
            const std::string& name = scene.nodes[index].name;

            if (!name.starts_with(k_toyPrefix) || !containsBall(name)
                || assets::subtreeInstances(scene, static_cast<uint32_t>(index)).empty())
            {
                continue;
            }

            toys.push_back(toyFromNode(scene, static_cast<uint32_t>(index)));
            hasBasketball = hasBasketball || name.starts_with(k_basketballName);
        }

        if (toys.size() > k_maxToys)
        {
            std::clog << "Game: " << toys.size() << " toys; only the first " << k_maxToys << " are kept\n";
            toys.resize(k_maxToys);
        }

        if (!hasBasketball && !toys.empty() && toys.size() < k_maxToys)
        {
            if (std::optional<Toy> basketball = madeBasketball(scene, toys.front()))
            {
                toys.push_back(std::move(*basketball));
                std::clog << "Game: no Toy_Basketball node; a basketball is made at the court\n";
            }
        }

        for (const Toy& toy : toys)
        {
            std::clog << "Game: toy " << toy.name << ", radius " << toy.radius << " m, at (" << toy.rest.x << ", "
                << toy.rest.y << ", " << toy.rest.z << ")\n";
        }

        return toys;
    }

    void hideToyInstances(std::span<const Toy> toys, SceneLevel& level, physics::PhysicsWorld& physics)
    {
        for (const Toy& toy : toys)
        {
            for (const ToyPart& part : toy.parts)
            {
                const uint32_t instance = part.instance;
                setInstancesVisible(level, physics, std::span<const uint32_t>(&instance, 1), false);
            }
        }
    }

    lockstep::Status spawnToys(std::vector<Toy>& toys, world::Entities& entities, lockstep::net::Replication& replication,
        physics::PhysicsWorld& physics)
    {
        for (std::size_t index = 0; index < toys.size(); ++index)
        {
            Toy& toy = toys[index];
            auto body = createToyBody(physics, toy, toy.rest);

            if (!body)
            {
                return body.error();
            }

            auto entity = entities.create(world::EntityDescription{.transform = math::Transform{.position = toy.rest},
                .domain = world::TimeDomain::World, .owner = world::EntityOwner::Host});

            if (!entity)
            {
                physics.destroyBody(body.value());
                return entity.error();
            }

            toy.body = body.value();
            toy.entity = entity.value();
            const auto archetype = static_cast<uint16_t>(static_cast<uint16_t>(Archetype::Toy) + index);

            if (auto tagged = replication.setArchetype(toy.entity, archetype); !tagged)
            {
                return tagged.error();
            }
        }

        return {};
    }

    void bindMirroredToys(std::vector<Toy>& toys, std::span<const lockstep::net::MirroredEntity> mirrored)
    {
        for (Toy& toy : toys)
        {
            toy.entity = {};
        }

        for (const lockstep::net::MirroredEntity& entity : mirrored)
        {
            const auto first = static_cast<uint16_t>(Archetype::Toy);

            if (entity.archetype >= first && entity.archetype - first < static_cast<int>(toys.size()))
            {
                toys[entity.archetype - first].entity = entity.entity;
            }
        }
    }

    std::optional<std::size_t> traceToy(std::span<const Toy> toys, const world::Entities& entities,
        const physics::PhysicsWorld& physics, const std::array<uint8_t, k_playerCount>& heldToy,
        const math::Vec3& eye, const math::Vec3& direction, float reach, physics::ControllerId ignore)
    {
        const std::optional<physics::RaycastHit> hit = physics.raycast(eye, direction, reach,
            physics::RaycastIgnore{.controller = ignore});

        for (std::size_t index = 0; index < toys.size(); ++index)
        {
            if (hit && !hit->body.isNull() && hit->body == toys[index].body)
            {
                return index;
            }
        }

        const float limit = std::min(hit ? hit->distance + k_boundsTolerance : reach, reach);
        std::optional<std::size_t> best;
        float bestDistance = limit;

        for (std::size_t index = 0; index < toys.size(); ++index)
        {
            const world::Entity* entity = entities.find(toys[index].entity);

            const bool isCarried = std::find(heldToy.begin(), heldToy.end(), index) != heldToy.end();

            if (entity == nullptr || isCarried)
            {
                continue;
            }

            const std::optional<float> enter = raySphere(eye, direction, entity->transform().position,
                toys[index].radius);

            if (enter && *enter <= bestDistance)
            {
                best = index;
                bestDistance = *enter;
            }
        }

        return best;
    }

    void pickUpToy(Toy& toy, physics::PhysicsWorld& physics)
    {
        physics.destroyBody(toy.body);
        toy.body = {};
    }

    lockstep::Status releaseToy(Toy& toy, const world::Entities& entities, physics::PhysicsWorld& physics,
        const math::Vec3& velocity)
    {
        const world::Entity* entity = entities.find(toy.entity);
        const math::Vec3 position = entity != nullptr ? entity->transform().position : toy.rest;
        auto body = createToyBody(physics, toy, position);

        if (!body)
        {
            return body.error();
        }

        toy.body = body.value();
        physics.setVelocity(toy.body, velocity);

        return {};
    }

    math::Vec3 holdPoint(const Toy& toy, const physics::PhysicsWorld& physics, const ToyPlayer& player)
    {
        const math::Vec3 forward = horizontal(player.direction);
        const float forwardLength = math::length(forward);
        const math::Vec3 flat = forwardLength > 0.0f ? forward * (1.0f / forwardLength) : math::Vec3{0.0f, 0.0f, -1.0f};
        const math::Vec3 wanted = player.eye + player.direction * k_holdDistance - math::Vec3{0.0f, k_holdDrop, 0.0f};
        const math::Vec3 aside = horizontal(wanted - player.eye);
        const float clearance = k_toyPawnRadius + toy.radius + k_holdClearance;
        const float outside = std::max(math::dot(aside, flat), clearance);
        const math::Vec3 target = wanted + flat * (outside - math::dot(aside, flat));
        const math::Vec3 reach = target - player.eye;
        const float distance = math::length(reach);

        if (distance <= 0.0f)
        {
            return target;
        }

        const math::Vec3 towards = reach * (1.0f / distance);
        const std::optional<physics::RaycastHit> hit = physics.raycast(player.eye, towards, distance + toy.radius,
            physics::RaycastIgnore{.controller = player.controller});

        if (!hit)
        {
            return target;
        }

        return player.eye + towards * std::max(hit->distance - toy.radius - k_holdClearance, 0.0f);
    }

    void updateToysAsHost(std::vector<Toy>& toys, const std::array<uint8_t, k_playerCount>& heldToy,
        const std::array<std::optional<ToyPlayer>, k_playerCount>& players, world::Entities& entities,
        physics::PhysicsWorld& physics, float worldDelta)
    {
        for (std::size_t index = 0; index < toys.size(); ++index)
        {
            Toy& toy = toys[index];
            world::Entity* entity = entities.find(toy.entity);

            if (entity == nullptr)
            {
                continue;
            }

            if (!toy.body.isNull())
            {
                updateFreeToy(toy, *entity, physics, players, worldDelta);
                continue;
            }

            for (std::size_t player = 0; player < k_playerCount; ++player)
            {
                if (heldToy[player] == index && players[player])
                {
                    entity->transform() = math::Transform{.position = holdPoint(toy, physics, *players[player]),
                        .rotation = entity->transform().rotation};
                }
            }
        }
    }

    void drawToys(lockstep::render::Renderer& renderer, const assets::Scene& scene, std::span<const Toy> toys,
        const world::Entities& entities, float alpha, std::optional<std::pair<std::size_t, math::Vec3>> localHeld)
    {
        for (std::size_t index = 0; index < toys.size(); ++index)
        {
            const world::Entity* entity = entities.find(toys[index].entity);

            if (entity == nullptr)
            {
                continue;
            }

            math::Transform frame = entity->interpolatedTransform(alpha);

            if (localHeld && localHeld->first == index)
            {
                frame.position = localHeld->second;
            }

            for (const ToyPart& part : toys[index].parts)
            {
                const assets::SceneMesh& mesh = scene.meshes[scene.instances[part.instance].mesh];
                renderer.submit(mesh.mesh, mesh.material, compose(frame, part.offset));
            }
        }
    }
} //namespace game
