#include "Lockstep/Assets/SceneCollision.hpp"

#include "Lockstep/Physics/PhysicsWorld.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <vector>

namespace lockstep::assets
{
    namespace
    {
        bool hasShape(const SceneMesh& mesh)
        {
            return !mesh.collisionIndices.empty() || mesh.collisionShape != k_noCollisionShape;
        }

        bool wantsCollider(const SceneMesh& mesh, const SceneInstance& instance, const SceneCollisionOptions& options)
        {
            if (!hasShape(mesh))
            {
                return false;
            }

            if (options.skipBlend && mesh.alphaMode == render::AlphaMode::Blend)
            {
                return false;
            }

            const math::Vec3 extent = math::size(instance.worldBounds);

            return extent.x >= options.minExtent || extent.y >= options.minExtent || extent.z >= options.minExtent;
        }

        Result<physics::BodyId> createInstanceBody(physics::PhysicsWorld& physics, const Scene& scene,
            const SceneMesh& mesh, const math::Transform& transform)
        {
            if (mesh.collisionShape < scene.collisionShapes.size())
            {
                const std::array<math::Transform, 1> transforms{transform};
                auto bodies = physics.createStaticMeshes(scene.collisionShapes[mesh.collisionShape], transforms);

                if (!bodies)
                {
                    return bodies.error();
                }

                if (bodies.value().empty())
                {
                    return Error{ErrorCode::PhysicsFailure, "addInstanceCollider: no body was created"};
                }

                return physics::BodyId{bodies.value().front()};
            }

            if (mesh.collisionIndices.empty())
            {
                return Error{ErrorCode::InvalidArgument,
                    "addInstanceCollider: the instance's mesh has no collision geometry"};
            }

            return physics.createStaticMesh(mesh.collisionPositions, mesh.collisionIndices, transform);
        }

        bool hasCollisionGeometry(const Scene& scene)
        {
            for (const SceneMesh& mesh : scene.meshes)
            {
                if (hasShape(mesh))
                {
                    return true;
                }
            }

            return false;
        }

        void addCookedBodies(physics::PhysicsWorld& physics, const Scene& scene,
            const std::vector<std::vector<uint32_t>>& instancesByShape, SceneCollision& collision)
        {
            std::vector<math::Transform> transforms;

            for (std::size_t shape = 0; shape < instancesByShape.size(); ++shape)
            {
                const std::vector<uint32_t>& instances = instancesByShape[shape];

                if (instances.empty())
                {
                    continue;
                }

                transforms.clear();

                for (const uint32_t instance : instances)
                {
                    transforms.push_back(scene.instances[instance].transform);
                }

                auto bodies = physics.createStaticMeshes(scene.collisionShapes[shape], transforms);

                if (!bodies)
                {
                    collision.skipped += static_cast<uint32_t>(instances.size());
                    continue;
                }

                collision.bodies.insert(collision.bodies.end(), bodies.value().begin(), bodies.value().end());
                collision.instances.insert(collision.instances.end(), instances.begin(), instances.end());

                for (const uint32_t instance : instances)
                {
                    collision.triangleCount += scene.meshes[scene.instances[instance].mesh].triangleCount;
                }
            }
        }
    } //namespace

    Result<SceneCollision> createSceneCollision(physics::PhysicsWorld& physics, const Scene& scene,
        const SceneCollisionOptions& options)
    {
        if (!scene.meshes.empty() && !hasCollisionGeometry(scene))
        {
            return Error{ErrorCode::InvalidArgument,
                "createSceneCollision: the scene was imported without collision geometry"};
        }

        const auto start = std::chrono::steady_clock::now();
        SceneCollision collision{};
        std::vector<std::vector<uint32_t>> instancesByShape(scene.collisionShapes.size());

        for (std::size_t index = 0; index < scene.instances.size(); ++index)
        {
            const SceneInstance& instance = scene.instances[index];
            const SceneMesh& mesh = scene.meshes[instance.mesh];

            if (!wantsCollider(mesh, instance, options))
            {
                ++collision.skipped;
                continue;
            }

            if (mesh.collisionShape < instancesByShape.size())
            {
                instancesByShape[mesh.collisionShape].push_back(static_cast<uint32_t>(index));
                continue;
            }

            if (mesh.collisionIndices.empty())
            {
                ++collision.skipped;
                continue;
            }

            auto body = physics.createStaticMesh(mesh.collisionPositions, mesh.collisionIndices, instance.transform);

            if (!body)
            {
                ++collision.skipped;
                continue;
            }

            collision.bodies.push_back(body.value());
            collision.instances.push_back(static_cast<uint32_t>(index));
            collision.triangleCount += mesh.collisionIndices.size() / 3;
        }

        addCookedBodies(physics, scene, instancesByShape, collision);
        collision.seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();

        return collision;
    }

    std::optional<uint32_t> findBodyInstance(const SceneCollision& collision, physics::BodyId body)
    {
        const auto found = std::find(collision.bodies.begin(), collision.bodies.end(), body);

        if (found == collision.bodies.end())
        {
            return std::nullopt;
        }

        return collision.instances[static_cast<std::size_t>(found - collision.bodies.begin())];
    }

    std::optional<physics::BodyId> findInstanceBody(const SceneCollision& collision, uint32_t instance)
    {
        const auto found = std::find(collision.instances.begin(), collision.instances.end(), instance);

        if (found == collision.instances.end())
        {
            return std::nullopt;
        }

        return collision.bodies[static_cast<std::size_t>(found - collision.instances.begin())];
    }

    void setInstanceTransform(Scene& scene, const SceneCollision& collision, physics::PhysicsWorld& physics,
        uint32_t instance, const math::Transform& transform)
    {
        if (instance >= scene.instances.size())
        {
            return;
        }

        SceneInstance& placed = scene.instances[instance];
        placed.transform = transform;
        placed.worldBounds = math::transformBounds(scene.meshes[placed.mesh].localBounds, transform);

        if (const std::optional<physics::BodyId> body = findInstanceBody(collision, instance))
        {
            physics.setTransform(*body, transform);
        }
    }

    Result<physics::BodyId> addInstanceCollider(SceneCollision& collision, physics::PhysicsWorld& physics,
        const Scene& scene, uint32_t instance)
    {
        if (instance >= scene.instances.size())
        {
            return Error{ErrorCode::InvalidArgument, "addInstanceCollider: instance out of range"};
        }

        if (const std::optional<physics::BodyId> existing = findInstanceBody(collision, instance))
        {
            return physics::BodyId{*existing};
        }

        const SceneInstance& placed = scene.instances[instance];
        const SceneMesh& mesh = scene.meshes[placed.mesh];
        auto body = createInstanceBody(physics, scene, mesh, placed.transform);

        if (!body)
        {
            return body.error();
        }

        collision.bodies.push_back(body.value());
        collision.instances.push_back(instance);
        collision.triangleCount += mesh.triangleCount;

        return physics::BodyId{body.value()};
    }

    bool removeInstanceCollider(SceneCollision& collision, physics::PhysicsWorld& physics, uint32_t instance)
    {
        const auto found = std::find(collision.instances.begin(), collision.instances.end(), instance);

        if (found == collision.instances.end())
        {
            return false;
        }

        const auto offset = found - collision.instances.begin();
        physics.destroyBody(collision.bodies[static_cast<std::size_t>(offset)]);
        collision.bodies.erase(collision.bodies.begin() + offset);
        collision.instances.erase(found);

        return true;
    }

    void destroySceneCollision(physics::PhysicsWorld& physics, SceneCollision& collision)
    {
        for (const physics::BodyId body : collision.bodies)
        {
            physics.destroyBody(body);
        }

        collision = SceneCollision{};
    }

    void releaseScene(render::Renderer& renderer, physics::PhysicsWorld& physics, Scene& scene,
        SceneCollision& collision)
    {
        destroySceneCollision(physics, collision);
        releaseScene(renderer, scene);
    }
} //namespace lockstep::assets
