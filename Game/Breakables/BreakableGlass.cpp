#include "Breakables/BreakableGlass.hpp"

#include "Level/SceneLevel.hpp"

#include <Lockstep/Assets/Scene.hpp>
#include <Lockstep/Assets/SceneCollision.hpp>
#include <Lockstep/Core/Bounds.hpp>
#include <Lockstep/Core/Transform.hpp>

#include <algorithm>
#include <array>
#include <iostream>

namespace game
{
    namespace
    {
        namespace assets = lockstep::assets;

        void measurePane(GlassPane& pane, const assets::Scene& scene)
        {
            const assets::SceneInstance& instance = scene.instances[pane.instances.front()];
            const lockstep::math::Aabb& local = scene.meshes[instance.mesh].localBounds;
            const math::Mat4 matrix = lockstep::math::toMatrix(instance.transform);
            const math::Vec3 half = lockstep::math::size(local) * 0.5f;
            pane.centre = math::transformPoint(matrix, lockstep::math::center(local));

            std::array<math::Vec3, 3> axes{math::transformDirection(matrix, math::Vec3{half.x, 0.0f, 0.0f}),
                math::transformDirection(matrix, math::Vec3{0.0f, half.y, 0.0f}),
                math::transformDirection(matrix, math::Vec3{0.0f, 0.0f, half.z})};
            std::sort(axes.begin(), axes.end(), [](const math::Vec3& left, const math::Vec3& right)
            {
                return math::lengthSquared(left) < math::lengthSquared(right);
            });

            const float thickness = math::length(axes[0]);
            pane.normal = thickness > 0.0f ? axes[0] * (1.0f / thickness) : math::normalize(math::cross(axes[1], axes[2]));
            pane.halfU = axes[1];
            pane.halfV = axes[2];
        }

        void giveColliders(const GlassPane& pane, SceneLevel& level, lockstep::physics::PhysicsWorld& physics)
        {
            for (const uint32_t instance : pane.instances)
            {
                if (auto added = assets::addInstanceCollider(level.collision, physics, level.scene, instance); !added)
                {
                    std::clog << "Game: glass pane '" << pane.name << "' has no collider: " << added.error().message
                              << '\n';
                }
            }
        }
    } //namespace

    BreakableGlass findGlassPanes(SceneLevel& level, lockstep::physics::PhysicsWorld& physics)
    {
        BreakableGlass glass{};
        const assets::Scene& scene = level.scene;

        for (uint32_t node = 0; node < scene.nodes.size() && glass.panes.size() < k_maxGlassPanes; ++node)
        {
            if (!scene.nodes[node].name.starts_with(k_glassPanePrefix))
            {
                continue;
            }

            GlassPane pane{.name = scene.nodes[node].name, .instances = assets::subtreeInstances(scene, node)};

            if (pane.instances.empty())
            {
                std::clog << "Game: glass pane '" << pane.name << "' has no mesh, skipped\n";
                continue;
            }

            measurePane(pane, scene);
            giveColliders(pane, level, physics);
            glass.panes.push_back(std::move(pane));
        }

        glass.isBroken.assign(glass.panes.size(), false);

        if (!glass.panes.empty())
        {
            std::clog << "Game: " << glass.panes.size() << " breakable glass panes\n";
        }

        return glass;
    }

    std::optional<std::size_t> findIntactPane(const BreakableGlass& glass, const SceneLevel& level,
        lockstep::physics::BodyId body)
    {
        if (glass.panes.empty() || body.isNull())
        {
            return std::nullopt;
        }

        const std::optional<uint32_t> instance = assets::findBodyInstance(level.collision, body);

        if (!instance)
        {
            return std::nullopt;
        }

        for (std::size_t index = 0; index < glass.panes.size(); ++index)
        {
            const std::vector<uint32_t>& instances = glass.panes[index].instances;

            if (!glass.isBroken[index] && std::find(instances.begin(), instances.end(), *instance) != instances.end())
            {
                return index;
            }
        }

        return std::nullopt;
    }

    bool applyGlassBreak(BreakableGlass& glass, SceneLevel& level, lockstep::physics::PhysicsWorld& physics,
        const GlassBreak& hit)
    {
        if (hit.pane >= glass.panes.size() || glass.isBroken[hit.pane])
        {
            return false;
        }

        glass.isBroken[hit.pane] = true;

        for (const uint32_t instance : glass.panes[hit.pane].instances)
        {
            level.scene.instances[instance].isVisible = false;
            assets::removeInstanceCollider(level.collision, physics, instance);
        }

        glass.shown.push_back(hit);

        return true;
    }

    bool shatterGlass(BreakableGlass& glass, SceneLevel& level, lockstep::physics::PhysicsWorld& physics,
        lockstep::physics::BodyId body, const math::Vec3& point, const math::Vec3& direction)
    {
        const std::optional<std::size_t> pane = findIntactPane(glass, level, body);

        if (!pane)
        {
            return false;
        }

        const GlassBreak hit{.pane = static_cast<uint16_t>(*pane), .point = point, .direction = direction};

        if (!applyGlassBreak(glass, level, physics, hit))
        {
            return false;
        }

        glass.unsent.push_back(hit);
        std::clog << "Game: glass pane '" << glass.panes[*pane].name << "' shattered (" << brokenPaneCount(glass)
                  << " of " << glass.panes.size() << " broken)\n";

        return true;
    }

    std::size_t brokenPaneCount(const BreakableGlass& glass)
    {
        return static_cast<std::size_t>(std::count(glass.isBroken.begin(), glass.isBroken.end(), true));
    }
} //namespace game
