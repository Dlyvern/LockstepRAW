#ifndef LOCKSTEP_GAME_PLAYER_LOCAL_PLAYER_HPP
#define LOCKSTEP_GAME_PLAYER_LOCAL_PLAYER_HPP

#include "Characters/PawnCharacters.hpp"
#include "Weapons/WeaponStance.hpp"

#include <Lockstep/App/GameApi.hpp>
#include <Lockstep/Physics/PhysicsTypes.hpp>
#include <Lockstep/World/Entities.hpp>

#include <cstddef>

namespace game
{
    // engine:  Engine services (the session role).
    // Returns: the pawn index of this machine's player (k_clientPawn on a client, else k_hostPawn).
    [[nodiscard]] std::size_t localPawnIndex(const lockstep::app::EngineContext& engine);

    // engine:  Engine services (the session role).
    // Returns: the entity owner of this machine's player (Player2 on a client, else Player1).
    [[nodiscard]] lockstep::world::EntityOwner localOwner(const lockstep::app::EngineContext& engine);

    // owner:   a pawn entity's owner.
    // Returns: that player's pawn index.
    [[nodiscard]] std::size_t ownerPawnIndex(lockstep::world::EntityOwner owner);

    // How another player's pawn is drawn, from their replicated status.
    // state:   the game state.
    // owner:   the pawn entity's owner.
    // Returns: armed, aiming and lowered as last known.
    [[nodiscard]] PawnLook remotePawnLook(const lockstep::app::GameState& state, lockstep::world::EntityOwner owner);

    // How the local pawn is drawn: armed from the replicated status, aiming and lowered from this
    // machine's newest command (no replication delay).
    // state:   the game state.
    // engine:  Engine services.
    // Returns: the look.
    [[nodiscard]] PawnLook localPawnLook(const lockstep::app::GameState& state,
        const lockstep::app::EngineContext& engine);

    // state:   the game state.
    // engine:  Engine services.
    // Returns: true while the local player carries a toy (as the room state last said).
    [[nodiscard]] bool isHoldingToy(const lockstep::app::GameState& state, const lockstep::app::EngineContext& engine);

    // state:   the game state.
    // engine:  Engine services.
    // Returns: the weapon zone the local pawn (host pawn or the client's predicted one) stands in,
    //          Open while it has none.
    [[nodiscard]] WeaponZone localWeaponZone(const lockstep::app::GameState& state,
        const lockstep::app::EngineContext& engine);

    // state:   the game state.
    // engine:  Engine services.
    // Returns: the local pawn's controller (the client's predicted one on a client); null if none.
    [[nodiscard]] lockstep::physics::ControllerId localController(const lockstep::app::GameState& state,
        const lockstep::app::EngineContext& engine);
} //namespace game

#endif //LOCKSTEP_GAME_PLAYER_LOCAL_PLAYER_HPP
