#include "Interaction/Interactables.hpp"

#include <Lockstep/Physics/PhysicsWorld.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace game
{
    namespace
    {
        namespace assets = lockstep::assets;
        namespace physics = lockstep::physics;

        constexpr float k_parallelEpsilon{1e-6f};
        constexpr float k_boundsTolerance{0.05f};

        bool hasPrefix(std::string_view name, std::span<const std::string_view> prefixes)
        {
            for (const std::string_view prefix : prefixes)
            {
                if (name.starts_with(prefix))
                {
                    return true;
                }
            }

            return false;
        }

        std::optional<float> rayEntersBounds(const math::Vec3& origin, const math::Vec3& direction, const math::Aabb& box)
        {
            float enter = 0.0f;
            float exit = std::numeric_limits<float>::max();
            const std::array<float, 3> start{origin.x, origin.y, origin.z};
            const std::array<float, 3> step{direction.x, direction.y, direction.z};
            const std::array<float, 3> low{box.min.x, box.min.y, box.min.z};
            const std::array<float, 3> high{box.max.x, box.max.y, box.max.z};

            for (std::size_t axis = 0; axis < 3; ++axis)
            {
                if (std::abs(step[axis]) < k_parallelEpsilon)
                {
                    if (start[axis] < low[axis] || start[axis] > high[axis])
                    {
                        return std::nullopt;
                    }

                    continue;
                }

                const float first = (low[axis] - start[axis]) / step[axis];
                const float second = (high[axis] - start[axis]) / step[axis];
                enter = std::max(enter, std::min(first, second));
                exit = std::min(exit, std::max(first, second));
            }

            return enter <= exit ? std::optional<float>(enter) : std::nullopt;
        }

        std::optional<LookTarget> hitInteractable(std::span<const Interactable> interactables, const SceneLevel& level,
            const physics::RaycastHit& hit)
        {
            const std::optional<uint32_t> instance = assets::findBodyInstance(level.collision, hit.body);

            if (!instance)
            {
                return std::nullopt;
            }

            const uint32_t hitNode = level.scene.instances[*instance].node;
            std::optional<LookTarget> best;

            for (std::size_t index = 0; index < interactables.size(); ++index)
            {
                const uint32_t node = interactables[index].node;

                if (assets::isNodeInSubtree(level.scene, hitNode, node)
                    && (!best || node > interactables[best->interactable].node))
                {
                    best = LookTarget{.interactable = index, .distance = hit.distance};
                }
            }

            return best;
        }

        std::optional<LookTarget> nearestBounds(std::span<const Interactable> interactables, const SceneLevel& level,
            const math::Vec3& eye, const math::Vec3& direction, float limit)
        {
            std::optional<LookTarget> best;

            for (std::size_t index = 0; index < interactables.size(); ++index)
            {
                for (const uint32_t instance : interactables[index].instances)
                {
                    const assets::SceneInstance& placed = level.scene.instances[instance];

                    if (!placed.isVisible)
                    {
                        continue;
                    }

                    const std::optional<float> enter = rayEntersBounds(eye, direction, placed.worldBounds);

                    if (enter && *enter <= limit && (!best || *enter < best->distance))
                    {
                        best = LookTarget{.interactable = index, .distance = *enter};
                    }
                }
            }

            return best;
        }
    } //namespace

    std::vector<Interactable> findInteractables(const assets::Scene& scene, std::span<const std::string_view> prefixes)
    {
        std::vector<Interactable> interactables;

        for (std::size_t index = 0; index < scene.nodes.size(); ++index)
        {
            const assets::SceneNode& node = scene.nodes[index];

            if (!hasPrefix(node.name, prefixes))
            {
                continue;
            }

            const auto nodeIndex = static_cast<uint32_t>(index);
            std::vector<uint32_t> instances = assets::subtreeInstances(scene, nodeIndex);

            if (!instances.empty())
            {
                interactables.push_back(Interactable{.name = node.name, .node = nodeIndex,
                    .instances = std::move(instances)});
            }
        }

        return interactables;
    }

    std::optional<LookTarget> traceInteractable(std::span<const Interactable> interactables, const SceneLevel& level,
        const physics::PhysicsWorld& physics, const math::Vec3& eye, const math::Vec3& direction, float reach,
        physics::ControllerId ignore)
    {
        const std::optional<physics::RaycastHit> hit = physics.raycast(eye, direction, reach,
            physics::RaycastIgnore{.controller = ignore});

        const float blockedAt = hit ? hit->distance : reach;

        if (hit && !hit->body.isNull())
        {
            if (const std::optional<LookTarget> direct = hitInteractable(interactables, level, *hit))
            {
                return direct;
            }
        }

        return nearestBounds(interactables, level, eye, direction, std::min(blockedAt + k_boundsTolerance, reach));
    }

    std::optional<std::size_t> findInteractable(std::span<const Interactable> interactables, std::string_view name)
    {
        for (std::size_t index = 0; index < interactables.size(); ++index)
        {
            if (interactables[index].name == name)
            {
                return index;
            }
        }

        return std::nullopt;
    }

    math::Aabb interactableBounds(const assets::Scene& scene, const Interactable& interactable)
    {
        math::Aabb bounds{};

        for (const uint32_t instance : interactable.instances)
        {
            math::expand(bounds, scene.instances[instance].worldBounds);
        }

        return bounds;
    }
} //namespace game
