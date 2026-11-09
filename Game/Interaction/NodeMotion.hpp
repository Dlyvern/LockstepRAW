#ifndef LOCKSTEP_GAME_INTERACTION_NODE_MOTION_HPP
#define LOCKSTEP_GAME_INTERACTION_NODE_MOTION_HPP

#include "Level/SceneLevel.hpp"

#include <Lockstep/Core/Math.hpp>
#include <Lockstep/Core/Transform.hpp>

#include <cstdint>
#include <span>
#include <vector>

namespace lockstep::physics
{
    class PhysicsWorld;
} //namespace lockstep::physics

namespace game
{
    // A rotation axis for a swinging part (door, locker door, lid).
    // pivot: a point on the axis, in world space.
    // axis:  unit axis; a positive angle opens the part.
    struct Hinge
    {
        math::Vec3 pivot{0.0f, 0.0f, 0.0f};
        math::Vec3 axis{0.0f, 1.0f, 0.0f};
    };

    // A group of level instances moved together as one rigid part, from its rest pose. Moving
    // it moves what is drawn and the colliders.
    // instances: indices into Scene::instances.
    // rest:      each instance's transform when captured.
    // angle:     the rotation last applied with rotateNodeMotion(), in radians.
    struct NodeMotion
    {
        std::vector<uint32_t> instances;
        std::vector<math::Transform> rest;
        float angle{0.0f};
    };

    // Remembers the rest pose of a group of instances.
    // scene:     the level's scene.
    // instances: indices into Scene::instances, e.g. Interactable::instances.
    // Returns:   the motion, at angle 0.
    [[nodiscard]] NodeMotion captureNodeMotion(const lockstep::assets::Scene& scene, std::span<const uint32_t> instances);

    // Rotates the group around a hinge, from its rest pose, moving the colliders too. Does
    // nothing when `angle` equals the angle already applied.
    // motion: the group; its angle is updated.
    // level:  the level (scene and colliders).
    // physics: the physics world the colliders live in.
    // hinge:  the axis.
    // angle:  radians from the rest pose.
    void rotateNodeMotion(NodeMotion& motion, SceneLevel& level, lockstep::physics::PhysicsWorld& physics,
        const Hinge& hinge, float angle);

    // A hinge on a node placed on the hinge line: through the node's origin, along its local up
    // axis (glTF +Y, Blender +Z).
    // scene: the level's scene.
    // node:  index into Scene::nodes.
    // Returns: the hinge; its direction still needs orientHinge().
    [[nodiscard]] Hinge hingeAtNode(const lockstep::assets::Scene& scene, uint32_t node);

    // A hinge found from a door's bounds: in the node's local frame, the vertical axis is the one
    // closest to world up, the door's width the longer of the other two, and the hinge is the
    // vertical edge of the bounds nearest the node's origin, centred in the door's thickness.
    // scene:     the level's scene.
    // node:      index into Scene::nodes of the door.
    // instances: the door's instances.
    // Returns:   the hinge; its direction still needs orientHinge().
    [[nodiscard]] Hinge hingeFromBounds(const lockstep::assets::Scene& scene, uint32_t node,
        std::span<const uint32_t> instances);

    // A hinge on a door's local west edge: along the node's local up axis, through the vertical
    // edge of its bounds on the node's local -X side, centred in the door's thickness (local Z).
    // For a door authored standing open whose node sits at its centre.
    // scene:     the level's scene.
    // node:      index into Scene::nodes of the door.
    // instances: the door's instances.
    // Returns:   the hinge, axis along the node's local up.
    [[nodiscard]] Hinge hingeAtWestEdge(const lockstep::assets::Scene& scene, uint32_t node,
        std::span<const uint32_t> instances);

    // How far a node is turned around the world up axis: the angle from world +X to its local +X
    // seen from above, positive counter-clockwise (the angle angleAxis() turns by around +Y).
    // scene:   the level's scene.
    // node:    index into Scene::nodes.
    // Returns: the angle in radians, in [-pi, pi].
    [[nodiscard]] float nodeYaw(const lockstep::assets::Scene& scene, uint32_t node);

    // Flips a hinge's axis so that opening moves the part's centre away from a point (the body
    // of a locker, or the room a door leads out of).
    // hinge:     the hinge, updated.
    // scene:     the level's scene.
    // instances: the part's instances.
    // awayFrom:  the point to swing away from.
    void orientHinge(Hinge& hinge, const lockstep::assets::Scene& scene, std::span<const uint32_t> instances,
        const math::Vec3& awayFrom);

    // Shows or hides a group of instances. Hiding also removes their colliders for good.
    // level:     the level (scene and colliders).
    // physics:   the physics world the colliders live in.
    // instances: indices into Scene::instances.
    // isVisible: whether to draw them.
    void setInstancesVisible(SceneLevel& level, lockstep::physics::PhysicsWorld& physics,
        std::span<const uint32_t> instances, bool isVisible);
} //namespace game

#endif //LOCKSTEP_GAME_INTERACTION_NODE_MOTION_HPP
