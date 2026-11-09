#ifndef LOCKSTEP_GAME_SIMULATION_STATION_SIMULATION_HPP
#define LOCKSTEP_GAME_SIMULATION_STATION_SIMULATION_HPP

#include <Lockstep/App/GameApi.hpp>
#include <Lockstep/Physics/PhysicsTypes.hpp>

#include <cstddef>
#include <span>

namespace game
{
    // Sets up the station mission when the loaded level is the police station (it has an
    // "Interact_Briefing" node). Host and client.
    // state:   the game state; gets `station` (and no enemy).
    // Returns: true if the level is the station.
    bool startStation(lockstep::app::GameState& state);

    // Host: handles one E press of a pawn in the station. Taking the weapon arms the pawn.
    // state:     the game state.
    // engine:    Engine services.
    // pawnIndex: whose press.
    void useStation(lockstep::app::GameState& state, lockstep::app::EngineContext& engine, std::size_t pawnIndex);

    // Host: a pawn's shot struck level geometry; knocks over the range target it hit, if any.
    // state:     the game state.
    // pawnIndex: the shooter.
    // body:      the struck body.
    void shootStation(lockstep::app::GameState& state, std::size_t pawnIndex, lockstep::physics::BodyId body);

    // Host, end of a tick: station rules, sending the station state to the client, applying it to
    // the level and the HUD.
    // state:  the game state.
    // engine: Engine services.
    void runStationHostTick(lockstep::app::GameState& state, lockstep::app::EngineContext& engine);

    // Client, end of a tick: applies the newest station state to the level and the HUD.
    // state:  the game state.
    // engine: Engine services.
    void runStationClientTick(lockstep::app::GameState& state, lockstep::app::EngineContext& engine);

    // Client: takes a Game message if it is a station state.
    // state:   the game state.
    // message: a received Game message.
    // Returns: true if it was a station state.
    bool receiveStationMessage(lockstep::app::GameState& state, std::span<const std::byte> message);

    // Host, dev: moves the station mission one step on as if the players did it: reports to the
    // chief, ends the briefing (its subtitles stop on both machines), gears up and arms every
    // present player, starts the mission.
    // state:  the game state.
    // engine: Engine services.
    void advanceStationForDev(lockstep::app::GameState& state, lockstep::app::EngineContext& engine);
} //namespace game

#endif //LOCKSTEP_GAME_SIMULATION_STATION_SIMULATION_HPP
