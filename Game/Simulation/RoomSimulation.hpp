#ifndef LOCKSTEP_GAME_SIMULATION_ROOM_SIMULATION_HPP
#define LOCKSTEP_GAME_SIMULATION_ROOM_SIMULATION_HPP

#include "Level/SceneLevel.hpp"

#include <Lockstep/App/GameApi.hpp>
#include <Lockstep/Core/Result.hpp>

#include <cstddef>
#include <filesystem>
#include <span>

namespace game
{
    // Sets up the apartment room if the loaded level has usable nodes (the host also spawns the
    // toys), and reads the Echo flags (`--echo-file`, `--echo-play`). Host and client.
    // state:  the game state; gets `room` and maybe `echo`.
    // engine: Engine services.
    // spawns: the level's spawn points.
    // Returns: success, or an error for a bad Echo flag, an unreadable `--echo-play` file or a toy
    //          that could not be spawned.
    [[nodiscard]] lockstep::Status startRoom(lockstep::app::GameState& state, lockstep::app::EngineContext& engine,
        const LevelSpawns& spawns);

    // Host: handles the E presses in a pawn's newest command (called after the command moved it).
    // Taking the gear from the locker arms the pawn. In the station level the presses go to the
    // station mission instead (useStation()).
    // state:     the game state.
    // engine:    Engine services.
    // pawnIndex: whose command.
    void interactWithRoom(lockstep::app::GameState& state, lockstep::app::EngineContext& engine, std::size_t pawnIndex);

    // Host: a pawn's shot while it carries a toy throws the toy instead.
    // state:     the game state.
    // engine:    Engine services.
    // pawnIndex: who shot.
    // Returns:   true if a toy was thrown (the shot is used up), false to fire normally.
    bool throwHeldToy(lockstep::app::GameState& state, lockstep::app::EngineContext& engine, std::size_t pawnIndex);

    // Host, end of a tick: room timers and animations, toys, the morning's phase, the Echo
    // recording, sending the room state to the client, applying it to the level and the HUD.
    // state:  the game state.
    // engine: Engine services.
    void runRoomHostTick(lockstep::app::GameState& state, lockstep::app::EngineContext& engine);

    // Client, end of a tick: finds the mirrored toys, applies the newest room state to the level
    // and updates the HUD.
    // state:  the game state.
    // engine: Engine services.
    void runRoomClientTick(lockstep::app::GameState& state, lockstep::app::EngineContext& engine);

    // Client: takes a Game message if it is a room state.
    // state:   the game state.
    // message: a received Game message.
    // Returns: true if it was a room state.
    bool receiveRoomMessage(lockstep::app::GameState& state, std::span<const std::byte> message);

    // Both: advances Echo playback, if one is running, by one tick.
    // state: the game state.
    void stepEcho(lockstep::app::GameState& state);

    // Starts playing an Echo recording as ghosts (dev command; also `--echo-play`).
    // state: the game state.
    // path:  the recording.
    // Returns: success, or the load error.
    [[nodiscard]] lockstep::Status playEcho(lockstep::app::GameState& state, const std::filesystem::path& path);
} //namespace game

#endif //LOCKSTEP_GAME_SIMULATION_ROOM_SIMULATION_HPP
