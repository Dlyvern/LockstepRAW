#ifndef LOCKSTEP_GAME_SHELL_GAME_SESSION_HPP
#define LOCKSTEP_GAME_SHELL_GAME_SESSION_HPP

#include "Level/SceneLevel.hpp"

#include <Lockstep/Core/Result.hpp>

#include <optional>

namespace lockstep::app
{
    struct EngineContext;
    struct GameState;
} //namespace lockstep::app

namespace game
{
    // Starts a game on this machine with the session as it is (hosting, connected or offline):
    // loads the level (or builds the arena), starts the level's ambience and, on the host, the
    // host simulation (Player1's pawn, the enemy; Player2's pawn follows when a client is
    // connected); a client builds the arena colliders and waits for the host's state.
    // state:  the game state (no level loaded).
    // engine: Engine services.
    // launch: the level, or nothing for the arena.
    // Returns: success, or the first load or spawn error.
    [[nodiscard]] lockstep::Status startGame(lockstep::app::GameState& state, lockstep::app::EngineContext& engine,
        const std::optional<SceneLaunch>& launch);

    // Ends the running game: removes pawns, the enemy, NPCs, the client's prediction and every
    // other entity, unloads the level, stops the game's sounds, resets the simulation and local
    // state and closes the session. Assets loaded at init (characters, sounds, effects) stay.
    // state:  the game state.
    // engine: Engine services.
    void endGame(lockstep::app::GameState& state, lockstep::app::EngineContext& engine);
} //namespace game

#endif //LOCKSTEP_GAME_SHELL_GAME_SESSION_HPP
