#ifndef LOCKSTEP_ASSETS_SCENE_COLLISION_HPP
#define LOCKSTEP_ASSETS_SCENE_COLLISION_HPP

#include "Lockstep/Assets/Scene.hpp"
#include "Lockstep/Core/Result.hpp"
#include "Lockstep/Physics/PhysicsTypes.hpp"

#include <cstdint>
#include <optional>
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
    // instances:     for each entry of `bodies`, the index into Scene::instances it was made for.
    // skipped:       instances left without a collider by the options or because cooking found
    //                no usable triangles.
    // triangleCount: triangles handed to PhysX.
    // seconds:       wall time spent cooking (imported scenes) and creating bodies.
    struct SceneCollision
    {
        std::vector<physics::BodyId> bodies;
        std::vector<uint32_t> instances;
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
    // Returns: the bodies (none for a scene without static meshes), or InvalidArgument if its
    //          static meshes have no collision geometry.
    [[nodiscard]] Result<SceneCollision> createSceneCollision(physics::PhysicsWorld& physics, const Scene& scene,
        const SceneCollisionOptions& options = {});

    // Finds which instance a body belongs to, e.g. to map a raycast hit back to a scene node.
    // collision: the scene's bodies.
    // body:      a body, e.g. RaycastHit::body.
    // Returns:   the index into Scene::instances, or nothing if `body` is not one of them.
    [[nodiscard]] std::optional<uint32_t> findBodyInstance(const SceneCollision& collision, physics::BodyId body);

    // Finds the body of an instance.
    // collision: the scene's bodies.
    // instance:  index into Scene::instances.
    // Returns:   its body, or nothing if the instance has no collider (or it was removed).
    [[nodiscard]] std::optional<physics::BodyId> findInstanceBody(const SceneCollision& collision, uint32_t instance);

    // Moves an instance at runtime: where it is drawn, its world bounds and its collider (static
    // bodies are teleported; the collider keeps its mesh scale). For doors, lids and props Game
    // animates; Game keeps the rest transforms if it wants to return.
    // scene:     the scene; instances[instance].transform and worldBounds are updated.
    // collision: the scene's bodies.
    // physics:   the physics world the bodies live in.
    // instance:  index into Scene::instances; out of range does nothing.
    // transform: the new world transform. Its scale should equal the old one: colliders keep the
    //            scale they were made with.
    void setInstanceTransform(Scene& scene, const SceneCollision& collision, physics::PhysicsWorld& physics,
        uint32_t instance, const math::Transform& transform);

    // Destroys an instance's collider, e.g. for a prop that was picked up and hidden. The
    // instance stays in the scene (hide it with SceneInstance::isVisible).
    // collision: the scene's bodies; the entry is removed.
    // physics:   the physics world the bodies live in.
    // instance:  index into Scene::instances.
    // Returns:   true if it had a collider.
    bool removeInstanceCollider(SceneCollision& collision, physics::PhysicsWorld& physics, uint32_t instance);

    // Destroys the bodies of createSceneCollision() and clears `collision`.
    // physics:   the physics world the bodies live in.
    // collision: the bodies.
    void destroySceneCollision(physics::PhysicsWorld& physics, SceneCollision& collision);
} //namespace lockstep::assets

#endif //LOCKSTEP_ASSETS_SCENE_COLLISION_HPP
