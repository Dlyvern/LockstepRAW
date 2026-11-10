#ifndef LOCKSTEP_GAME_PLAYER_PAWN_MANTLE_HPP
#define LOCKSTEP_GAME_PLAYER_PAWN_MANTLE_HPP

#include "Player/PawnMovement.hpp"
#include "Player/PlayerCommand.hpp"

#include <Lockstep/Physics/PhysicsTypes.hpp>

#include <optional>
#include <span>

namespace lockstep::physics
{
    class PhysicsWorld;
} //namespace lockstep::physics

namespace game
{
    // Starts a mantle when a buffered jump press meets a ledge: moving forward, something within
    // reach in front, a walkable top between PawnSettings::mantleMinHeight and mantleMaxHeight
    // above the feet, room above the pawn to rise and room on top for its capsule. Finds the ledge
    // with rays against shared geometry only (sharedRaycast()), so host and client agree. A thin
    // top (a railing, a low wall) becomes a vault: the pawn goes over and drops on the far side.
    // motion:     the pawn's motion; on success the mantle starts (mantleTicks, mantleHeight, the
    //             forward velocity) and the jump press is used up.
    // controller: the pawn's controller.
    // physics:    the physics world.
    // command:    the command of this tick.
    // settings:   mantle reach, heights and speeds.
    // delta:      the Player domain's delta for this tick, in seconds.
    // ignored:    bodies the rays pass through (the host's dynamic toys).
    // Returns:    true if a mantle started.
    bool tryStartMantle(PawnMotion& motion, lockstep::physics::ControllerId controller,
        const lockstep::physics::PhysicsWorld& physics, const PlayerCommand& command, const PawnSettings& settings,
        float delta, std::span<const lockstep::physics::BodyId> ignored);

    // Moves a mantling pawn for one tick: straight up to the ledge height, then forward onto it.
    // Ends when the forward part is done, or early if the rise is blocked.
    // motion:     the pawn's motion (mantleTicks > 0).
    // controller: the pawn's controller.
    // physics:    moves the controller.
    // settings:   mantle speeds.
    // delta:      the Player domain's delta for this tick, in seconds.
    // Returns:    what the move touched, or nothing for a stale controller.
    std::optional<lockstep::physics::ControllerMove> stepMantle(PawnMotion& motion,
        lockstep::physics::ControllerId controller, lockstep::physics::PhysicsWorld& physics,
        const PawnSettings& settings, float delta);
} //namespace game

#endif //LOCKSTEP_GAME_PLAYER_PAWN_MANTLE_HPP
