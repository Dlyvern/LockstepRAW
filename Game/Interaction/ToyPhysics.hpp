#ifndef LOCKSTEP_GAME_INTERACTION_TOY_PHYSICS_HPP
#define LOCKSTEP_GAME_INTERACTION_TOY_PHYSICS_HPP

#include "Interaction/Toys.hpp"

#include <Lockstep/Core/Math.hpp>
#include <Lockstep/Core/Result.hpp>
#include <Lockstep/Physics/PhysicsTypes.hpp>
#include <Lockstep/World/Entities.hpp>

#include <array>
#include <optional>

namespace lockstep::physics
{
    class PhysicsWorld;
} //namespace lockstep::physics

namespace game
{
    // Radius of a pawn's body as toys see it (kicks, holding clearance), in metres.
    inline constexpr float k_toyPawnRadius{0.4f};

    // Host: creates a toy's dynamic sphere body (ball density, friction and bounce).
    // physics:  the physics world.
    // toy:      the toy (its radius).
    // position: where the body's centre starts.
    // Returns:  the body, or the creation error.
    [[nodiscard]] lockstep::Result<lockstep::physics::BodyId> createToyBody(lockstep::physics::PhysicsWorld& physics,
        const Toy& toy, const math::Vec3& position);

    // Host: one tick of a free (not carried) toy: copies its body into its entity, lets players
    // walking into it kick it, rolls it to a stop (stopped dead once slow on the ground) and puts it
    // back at its rest position if it fell out of the level.
    // toy:        the toy; must have a body.
    // entity:     the toy's entity.
    // physics:    the physics world.
    // players:    per player, their pawn this tick, or nothing while absent.
    // worldDelta: World domain delta of this tick.
    void updateFreeToy(Toy& toy, lockstep::world::Entity& entity, lockstep::physics::PhysicsWorld& physics,
        const std::array<std::optional<ToyPlayer>, k_playerCount>& players, float worldDelta);
} //namespace game

#endif //LOCKSTEP_GAME_INTERACTION_TOY_PHYSICS_HPP
