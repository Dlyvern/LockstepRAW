#include "Lockstep/Assets/SceneCollision.hpp"

#include "Lockstep/Physics/PhysicsWorld.hpp"

#include <chrono>

namespace lockstep::assets
{
    namespace
    {
        bool wantsCollider(const SceneMesh& mesh, const SceneInstance& instance, const SceneCollisionOptions& options)
        {
            if (mesh.collisionIndices.empty())
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

        bool hasCollisionGeometry(const Scene& scene)
        {
            for (const SceneMesh& mesh : scene.meshes)
            {
                if (!mesh.collisionIndices.empty())
                {
                    return true;
                }
            }

            return false;
        }
    } //namespace

    Result<SceneCollision> createSceneCollision(physics::PhysicsWorld& physics, const Scene& scene,
        const SceneCollisionOptions& options)
    {
        if (!hasCollisionGeometry(scene))
        {
            return Error{ErrorCode::InvalidArgument,
                "createSceneCollision: the scene was imported without collision geometry"};
        }

        const auto start = std::chrono::steady_clock::now();
        SceneCollision collision{};

        for (const SceneInstance& instance : scene.instances)
        {
            const SceneMesh& mesh = scene.meshes[instance.mesh];

            if (!wantsCollider(mesh, instance, options))
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
            collision.triangleCount += mesh.collisionIndices.size() / 3;
        }

        collision.seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();

        return collision;
    }

    void destroySceneCollision(physics::PhysicsWorld& physics, SceneCollision& collision)
    {
        for (const physics::BodyId body : collision.bodies)
        {
            physics.destroyBody(body);
        }

        collision = SceneCollision{};
    }
} //namespace lockstep::assets
