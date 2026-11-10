#ifndef LOCKSTEP_GAME_PLAYER_PAWN_LEAN_HPP
#define LOCKSTEP_GAME_PLAYER_PAWN_LEAN_HPP

#include "Player/PawnMovement.hpp"
#include "Player/PlayerCommand.hpp"

#include <Lockstep/Core/Math.hpp>
#include <Lockstep/Physics/PhysicsTypes.hpp>

#include <span>

namespace lockstep::physics
{
    class PhysicsWorld;
} //namespace lockstep::physics

namespace game
{
    // Eases a pawn's lean towards what the command asks (none while running, sliding or mantling) and keeps the leaned head out of walls: rays from the upright eye and shoulder
    // towards the lean side shorten it. Part of stepPawn(); host and client run it alike.
    // motion:     the pawn's motion; `lean` is updated.
    // controller: the pawn's controller (its foot position and the rays' own capsule).
    // physics:    the physics world.
    // command:    the command of this tick.
    // settings:   lean distance, speed and wall clearance.
    // delta:      the Player domain's delta for this tick, in seconds.
    // ignored:    bodies the rays pass through (the host's dynamic toys).
    void updateLean(PawnMotion& motion, lockstep::physics::ControllerId controller,
        const lockstep::physics::PhysicsWorld& physics, const PlayerCommand& command, const PawnSettings& settings,
        float delta, std::span<const lockstep::physics::BodyId> ignored);

    // motion:   a pawn's motion.
    // settings: movement settings (lean distance).
    // Returns:  how far the eyes are moved sideways by the lean, metres (negative is left).
    [[nodiscard]] float leanOffset(const PawnMotion& motion, const PawnSettings& settings);
} //namespace game

#endif //LOCKSTEP_GAME_PLAYER_PAWN_LEAN_HPP
