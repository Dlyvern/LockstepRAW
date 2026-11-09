#ifndef LOCKSTEP_GAME_LEVEL_ROOM_DEV_PANEL_HPP
#define LOCKSTEP_GAME_LEVEL_ROOM_DEV_PANEL_HPP

#include <Lockstep/App/GameApi.hpp>

namespace game
{
#if LOCKSTEP_DEV_TOOLS
    // Draws the room section of the Game dev panel: the local prompt (stand-in for HUD text), the
    // shared room state, and the Echo recorder with play and stop commands.
    // state: the game state.
    void roomDevSection(lockstep::app::GameState& state);
#endif
} //namespace game

#endif //LOCKSTEP_GAME_LEVEL_ROOM_DEV_PANEL_HPP
