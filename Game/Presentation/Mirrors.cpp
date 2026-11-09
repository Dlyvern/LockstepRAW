#include "Presentation/Mirrors.hpp"

#include <Lockstep/Assets/Scene.hpp>
#include <Lockstep/Render/Renderer.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string_view>
#include <vector>

namespace game
{
    namespace
    {
        namespace render = lockstep::render;
        namespace assets = lockstep::assets;

        constexpr std::string_view k_mirrorNode{"Interact_Mirror"};
        constexpr float k_sameTransformTolerance{1e-4f};

        bool isSameTransform(const math::Transform& left, const math::Transform& right)
        {
            return math::length(left.position - right.position) < k_sameTransformTolerance
                && math::length(left.scale - right.scale) < k_sameTransformTolerance
                && std::abs(left.rotation.x * right.rotation.x + left.rotation.y * right.rotation.y
                    + left.rotation.z * right.rotation.z + left.rotation.w * right.rotation.w)
                    > 1.0f - k_sameTransformTolerance;
        }

        float flatness(const math::Aabb& bounds)
        {
            const math::Vec3 extent = bounds.max - bounds.min;
            const float largest = std::max(extent.x, std::max(extent.y, extent.z));

            return largest > 0.0f ? std::min(extent.x, std::min(extent.y, extent.z)) / largest : 1.0f;
        }

        std::vector<uint32_t> mirrorInstances(const assets::Scene& scene, uint32_t node)
        {
            std::vector<uint32_t> instances = assets::subtreeInstances(scene, node);

            if (!instances.empty())
            {
                return instances;
            }

            const math::Transform& placement = scene.nodes[node].transform;
            float flattest = 1.0f;

            for (uint32_t index = 0; index < scene.instances.size(); ++index)
            {
                const assets::SceneInstance& instance = scene.instances[index];
                const float candidate = flatness(scene.meshes[instance.mesh].localBounds);

                if (isSameTransform(instance.transform, placement) && candidate < flattest)
                {
                    flattest = candidate;
                    instances.assign(1, index);
                }
            }

            return instances;
        }
    } //namespace

    void presentMirrors(render::Renderer& renderer, const assets::Scene* scene, const Visuals& visuals,
        const std::optional<math::Transform>& localPawn, lockstep::world::EntityOwner localOwner)
    {
        if (scene == nullptr)
        {
            return;
        }

        const std::optional<uint32_t> node = assets::findNodeIndex(*scene, k_mirrorNode);

        if (!node)
        {
            return;
        }

        for (const uint32_t index : mirrorInstances(*scene, *node))
        {
            const assets::SceneInstance& instance = scene->instances[index];

            if (instance.isVisible)
            {
                renderer.submitReflector({.mesh = scene->meshes[instance.mesh].mesh, .transform = instance.transform,
                    .layers = render::k_defaultRenderLayer | k_mirrorOnlyLayer});
            }
        }

        if (localPawn)
        {
            renderer.submit(visuals.cube,
                localOwner == lockstep::world::EntityOwner::Player2 ? visuals.player2 : visuals.player1, *localPawn,
                k_mirrorOnlyLayer);
        }
    }
} //namespace game
