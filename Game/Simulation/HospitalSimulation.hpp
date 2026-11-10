#ifndef LOCKSTEP_GAME_SIMULATION_HOSPITAL_SIMULATION_HPP
#define LOCKSTEP_GAME_SIMULATION_HOSPITAL_SIMULATION_HPP

#include <Lockstep/App/GameApi.hpp>

#include <cstddef>
#include <span>

namespace game
{
    // Sets up the hospital if the loaded level has its markers (HospitalMission.hpp): players are
    // armed, the old melee enemy stays off. Host also: combat is active, the navigation grid is
    // built around the markers, the squad spawns; in dev builds `--dev-mercs` enables mercs and
    // `--dev-start-beat <Beat_name>` starts the players at that beat. Host and client.
    // state:   the game state (gets `hospital`).
    // engine:  Engine services.
    // Returns: true if this is a hospital-style level.
    bool startHospital(lockstep::app::GameState& state, lockstep::app::EngineContext& engine);

    // Host, end of a tick: beats reached (checkpoint, merc floor spawns when enabled), the lift's
    // fade and exit, the objective and lift prompt, and the hospital state message.
    // state:  the game state.
    // engine: Engine services.
    void runHospitalHostTick(lockstep::app::GameState& state, lockstep::app::EngineContext& engine);

    // Client, end of a tick: the objective, the lift prompt and the lift's fade.
    // state:  the game state.
    // engine: Engine services.
    void runHospitalClientTick(lockstep::app::GameState& state, lockstep::app::EngineContext& engine);

    // Client: takes a Game message if it is the hospital state.
    // state:   the game state.
    // message: a received Game message.
    // Returns: true if it was.
    bool receiveHospitalMessage(lockstep::app::GameState& state, std::span<const std::byte> message);

    // Host: a player pressed F in the hospital: the service lift within reach takes them down
    // (fade out; the next level loads if it is mounted, else they are told it isn't built).
    // state:     the game state.
    // engine:    Engine services.
    // pawnIndex: who pressed.
    void useHospital(lockstep::app::GameState& state, lockstep::app::EngineContext& engine, std::size_t pawnIndex);

    // Host: spawns one floor's merc group now, if it has not spawned yet.
    // state:  the game state.
    // engine: Engine services.
    // floor:  0 ground, 1 second, 2 third.
    void spawnMercFloor(lockstep::app::GameState& state, lockstep::app::EngineContext& engine, std::size_t floor);

    // Host: turns merc spawning on (the ground floor's group and those of floors whose beats were
    // reached spawn at once; later beats spawn theirs) or off (nothing more spawns).
    // state:   the game state.
    // engine:  Engine services.
    // enabled: whether mercs spawn.
    void setMercsEnabled(lockstep::app::GameState& state, lockstep::app::EngineContext& engine, bool enabled);

    // Host: removes every merc (bodies included); the squad stays. Floors may spawn again.
    // state:  the game state.
    // engine: Engine services.
    void removeMercs(lockstep::app::GameState& state, lockstep::app::EngineContext& engine);
} //namespace game

#endif //LOCKSTEP_GAME_SIMULATION_HOSPITAL_SIMULATION_HPP
