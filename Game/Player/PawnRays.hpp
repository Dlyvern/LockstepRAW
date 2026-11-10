#ifndef LOCKSTEP_GAME_PLAYER_PAWN_RAYS_HPP
#define LOCKSTEP_GAME_PLAYER_PAWN_RAYS_HPP

#include <Lockstep/Core/Math.hpp>
#include <Lockstep/Physics/PhysicsTypes.hpp>

#include <optional>
#include <span>

namespace lockstep::physics
{
    class PhysicsWorld;
} //namespace lockstep::physics

namespace game
{
    namespace math = lockstep::math;

    // Casts a ray that only hits geometry both machines have (level and arena colliders): it
    // passes through every character controller (pawns and the enemy exist on the host only, or
    // in other places) and through the listed bodies (the host's dynamic toys). Pawn movement uses
    // it so the host and a predicting client see the same hits.
    // physics:   the physics world.
    // origin:    ray start.
    // direction: unit ray direction.
    // distance:  longest hit distance.
    // self:      the pawn's own controller, never hit (the ray may start inside it).
    // ignored:   bodies to pass through.
    // Returns:   the closest hit on shared geometry, or nothing.
    [[nodiscard]] std::optional<lockstep::physics::RaycastHit> sharedRaycast(
        const lockstep::physics::PhysicsWorld& physics, const math::Vec3& origin, const math::Vec3& direction,
        float distance, lockstep::physics::ControllerId self, std::span<const lockstep::physics::BodyId> ignored);
} //namespace game

#endif //LOCKSTEP_GAME_PLAYER_PAWN_RAYS_HPP
