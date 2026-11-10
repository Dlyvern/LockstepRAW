#ifndef LOCKSTEP_GAME_HOSPITAL_HOSPITAL_DEV_PANEL_HPP
#define LOCKSTEP_GAME_HOSPITAL_HOSPITAL_DEV_PANEL_HPP

#if LOCKSTEP_DEV_TOOLS

#include <Lockstep/App/GameApi.hpp>

namespace game
{
    // Draws the combat sections of the Game dev panel. Host: merc spawning ("Spawn mercs", per
    // floor, remove, kill), test actions (down yourself, restart at the checkpoint), every NPC's
    // state, and the NPC, downed and ammo tunables. Client: own ammo and downed state.
    // state:  the game state.
    // engine: Engine services.
    void combatDevSection(lockstep::app::GameState& state, lockstep::app::EngineContext& engine);
} //namespace game

#endif

#endif //LOCKSTEP_GAME_HOSPITAL_HOSPITAL_DEV_PANEL_HPP
