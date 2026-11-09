#ifndef LOCKSTEP_GAME_SIMULATION_HOST_SIMULATION_HPP
#define LOCKSTEP_GAME_SIMULATION_HOST_SIMULATION_HPP

#include <Lockstep/App/GameApi.hpp>
#include <Lockstep/Core/Result.hpp>

#include <cstddef>

namespace game
{
    // Index of the host's (Player1's) pawn in GameState::Simulation::pawns.
    inline constexpr std::size_t k_hostPawn{0};

    // Index of the client's (Player2's) pawn in GameState::Simulation::pawns.
    inline constexpr std::size_t k_clientPawn{1};

    // Host and offline: creates the arena bodies, the host's pawn and the enemy.
    // state:   the game state.
    // engine:  Engine services.
    // Returns: success, or the first error.
    [[nodiscard]] lockstep::Status startHostSimulation(lockstep::app::GameState& state,
        lockstep::app::EngineContext& engine);

    // Host and offline: runs the game rules for one tick: joins and leaves, the client's inputs
    // in order (each moves its pawn and fires its shots, checked against the world as the client
    // saw it), bullet-time, the host's pawn and shots, the enemy, damage and respawns, the lag
    // compensation record, the health message and the client's pawn state for its prediction.
    // The host's own command must already be in state.local.command.
    // state:  the game state.
    // engine: Engine services.
    void runHostTick(lockstep::app::GameState& state, lockstep::app::EngineContext& engine);
} //namespace game

#endif //LOCKSTEP_GAME_SIMULATION_HOST_SIMULATION_HPP
