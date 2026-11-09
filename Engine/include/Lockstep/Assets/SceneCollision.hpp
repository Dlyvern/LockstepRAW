#ifndef LOCKSTEP_ASSETS_SCENE_COLLISION_HPP
#define LOCKSTEP_ASSETS_SCENE_COLLISION_HPP

#include "Lockstep/Assets/Scene.hpp"
#include "Lockstep/Core/Result.hpp"
#include "Lockstep/Physics/PhysicsTypes.hpp"

#include <cstdint>
#include <vector>

namespace lockstep::physics
{
    class PhysicsWorld;
} //namespace lockstep::physics

namespace lockstep::assets
{
    // Which scene instances get a collider.
    // skipBlend: skip instances whose material is alpha-blended (glass, decals).
    // minExtent: skip instances whose world bounds are smaller than this on every axis, in metres
    //            (cups, bottles, debris the player should walk through rather than up).
    struct SceneCollisionOptions
    {
        bool skipBlend{true};
        float minExtent{0.05f};
    };

    // The static bodies made for a scene.
    // bodies:        one static triangle-mesh body per collided instance.
    // skipped:       instances left without a collider by the options or because cooking found
    //                no usable triangles.
    // triangleCount: triangles handed to PhysX.
    // seconds:       wall time spent cooking (imported scenes) and creating bodies.
    struct SceneCollision
    {
        std::vector<physics::BodyId> bodies;
        uint32_t skipped{0};
        uint64_t triangleCount{0};
        double seconds{0.0};
    };

    // Builds static triangle-mesh colliders for a scene's instances, making it walkable. Meshes
    // of a cooked scene use their pre-cooked shape, loaded once and shared by its instances;
    // meshes of an imported glTF are cooked here, per instance.
    // physics: the physics world.
    // scene:   a cooked scene, or one imported with SceneImportOptions::keepCollisionGeometry.
    // options: which instances to skip.
    // Returns: the bodies, or InvalidArgument if the scene has no collision geometry.
    [[nodiscard]] Result<SceneCollision> createSceneCollision(physics::PhysicsWorld& physics, const Scene& scene,
        const SceneCollisionOptions& options = {});

    // Destroys the bodies of createSceneCollision() and clears `collision`.
    // physics:   the physics world the bodies live in.
    // collision: the bodies.
    void destroySceneCollision(physics::PhysicsWorld& physics, SceneCollision& collision);
} //namespace lockstep::assets

#endif //LOCKSTEP_ASSETS_SCENE_COLLISION_HPP
