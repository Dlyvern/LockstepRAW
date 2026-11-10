#ifndef LOCKSTEP_GAME_GORE_GORE_DEV_PANEL_HPP
#define LOCKSTEP_GAME_GORE_GORE_DEV_PANEL_HPP

#include <Lockstep/App/GameApi.hpp>

namespace game
{
#if LOCKSTEP_DEV_TOOLS
    // Draws the gore section of the Game dev panel: the sever rules (host), the blood and piece
    // tuning, counters, and buttons to sever a random limb of the NPC nearest the camera or clear
    // all blood. On a client the button only shows the cut on this machine.
    // state:  the game state.
    // engine: Engine services.
    void goreDevSection(lockstep::app::GameState& state, lockstep::app::EngineContext& engine);
#endif
} //namespace game

#endif //LOCKSTEP_GAME_GORE_GORE_DEV_PANEL_HPP
