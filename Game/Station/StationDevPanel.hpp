#ifndef LOCKSTEP_GAME_STATION_STATION_DEV_PANEL_HPP
#define LOCKSTEP_GAME_STATION_STATION_DEV_PANEL_HPP

#include <Lockstep/App/GameApi.hpp>

namespace game
{
#if LOCKSTEP_DEV_TOOLS
    // Draws the level flow and station section of the Game dev panel: the loaded and next level,
    // a host button that finishes the morning (the drive and the switch to the next level
    // follow), the station state and a host button that advances the mission one step
    // (advanceStationForDev()).
    // state:  the game state.
    // engine: Engine services.
    void stationDevSection(lockstep::app::GameState& state, lockstep::app::EngineContext& engine);
#endif
} //namespace game

#endif //LOCKSTEP_GAME_STATION_STATION_DEV_PANEL_HPP
