#ifndef LOCKSTEP_GAME_PLAYER_PAWN_VAULT_HPP
#define LOCKSTEP_GAME_PLAYER_PAWN_VAULT_HPP

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
    // Starts a vault when a buffered jump press meets a thin waist-high obstacle: moving forward
    // (walking, running or sliding) on the ground, something within reach in front whose top is
    // between VaultSettings::minHeight and maxHeight above the feet and which ends within
    // maxThickness, a floor on the far side, and room for the capsule over the top and at the
    // landing. Uses rays against shared geometry only (sharedRaycast()), so host and client agree.
    // motion:     the pawn's motion; on success the vault starts (mantleTicks, mantleHeight,
    //             isVaulting, the crossing velocity), a slide ends and the jump press is used up.
    // controller: the pawn's controller.
    // physics:    the physics world.
    // command:    the command of this tick.
    // settings:   vault settings (PawnSettings::vault).
    // delta:      the Player domain's delta for this tick, in seconds.
    // ignored:    bodies the rays pass through (the host's dynamic toys).
    // Returns:    true if a vault started.
    bool tryStartVault(PawnMotion& motion, lockstep::physics::ControllerId controller,
        const lockstep::physics::PhysicsWorld& physics, const PlayerCommand& command, const PawnSettings& settings,
        float delta, std::span<const lockstep::physics::BodyId> ignored);

    // Moves a vaulting pawn for one tick: hops up to just above the obstacle, crosses it, then
    // lets it drop on the far side keeping VaultSettings::exitScale of its speed. Ends early if
    // the hop or the crossing is blocked. The weapon can't fire while vaulting.
    // motion:     the pawn's motion (isVaulting()).
    // controller: the pawn's controller.
    // physics:    moves the controller.
    // settings:   vault settings.
    // delta:      the Player domain's delta for this tick, in seconds.
    // Returns:    what the move touched, or nothing for a stale controller.
    std::optional<lockstep::physics::ControllerMove> stepVault(PawnMotion& motion,
        lockstep::physics::ControllerId controller, lockstep::physics::PhysicsWorld& physics,
        const PawnSettings& settings, float delta);
} //namespace game

#endif //LOCKSTEP_GAME_PLAYER_PAWN_VAULT_HPP
