#ifndef LOCKSTEP_GAME_DEV_PANEL_HPP
#define LOCKSTEP_GAME_DEV_PANEL_HPP

#include <Lockstep/App/GameApi.hpp>

namespace game
{
#if LOCKSTEP_DEV_TOOLS
    // Draws the Game dev panel: tunables, time domains, pawn positions, enemy state and shot
    // counters. Used as GameApi::devToolsPanels.
    // state:  the game state.
    // engine: Engine services.
    void devToolsPanels(lockstep::app::GameState& state, lockstep::app::EngineContext& engine);
#endif
} //namespace game

#endif //LOCKSTEP_GAME_DEV_PANEL_HPP
