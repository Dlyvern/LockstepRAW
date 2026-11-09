#ifndef LOCKSTEP_GAME_INTERACTION_INTERACTABLES_HPP
#define LOCKSTEP_GAME_INTERACTION_INTERACTABLES_HPP

#include "Level/SceneLevel.hpp"

#include <Lockstep/Core/Bounds.hpp>
#include <Lockstep/Core/Math.hpp>
#include <Lockstep/Physics/PhysicsTypes.hpp>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace lockstep::physics
{
    class PhysicsWorld;
} //namespace lockstep::physics

namespace game
{
    // How far a player can reach to use something, in metres from the eyes.
    inline constexpr float k_interactReach{2.0f};

    // Something in a level a player can look at and use: a named scene node found by its name
    // prefix (e.g. "Interact_"), and every instance drawn under it.
    // name:      the node's name as authored.
    // node:      index into Scene::nodes.
    // instances: indices into Scene::instances under the node.
    struct Interactable
    {
        std::string name;
        uint32_t node{0};
        std::vector<uint32_t> instances;
    };

    // What a player looks at.
    // interactable: index into the interactable list.
    // distance:     metres from the eyes to the surface looked at.
    struct LookTarget
    {
        std::size_t interactable{0};
        float distance{0.0f};
    };

    // Finds every node whose name starts with one of `prefixes` (case-sensitive), with the
    // instances under it. Nodes without instances (markers) are left out.
    // scene:    the level's scene; needs the node hierarchy (a level cooked with format 2).
    // prefixes: name prefixes that make a node usable, e.g. "Interact_".
    // Returns:  the interactables, in node order.
    [[nodiscard]] std::vector<Interactable> findInteractables(const lockstep::assets::Scene& scene,
        std::span<const std::string_view> prefixes);

    // Casts a ray from the eyes and tells which interactable it looks at. A collider hit that
    // belongs to an interactable wins; otherwise the nearest interactable whose instance bounds the
    // ray enters before the first collider (so see-through parts such as a vented locker door,
    // whose panel has no collider, still count). Anything else in the way blocks it.
    // interactables: the level's interactables.
    // level:         the level (scene and colliders).
    // physics:       the physics world for the raycast.
    // eye:           ray start.
    // direction:     unit look direction.
    // reach:         longest distance.
    // ignore:        the looking pawn's controller.
    // Returns:       the target, or nothing.
    [[nodiscard]] std::optional<LookTarget> traceInteractable(std::span<const Interactable> interactables,
        const SceneLevel& level, const lockstep::physics::PhysicsWorld& physics, const math::Vec3& eye,
        const math::Vec3& direction, float reach, lockstep::physics::ControllerId ignore);

    // interactables: the level's interactables.
    // name:          the exact node name.
    // Returns:       the index of the interactable with that name, or nothing.
    [[nodiscard]] std::optional<std::size_t> findInteractable(std::span<const Interactable> interactables,
        std::string_view name);

    // scene:        the level's scene.
    // interactable: an interactable.
    // Returns:      the world bounds of its instances.
    [[nodiscard]] math::Aabb interactableBounds(const lockstep::assets::Scene& scene, const Interactable& interactable);
} //namespace game

#endif //LOCKSTEP_GAME_INTERACTION_INTERACTABLES_HPP
