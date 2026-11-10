#ifndef LOCKSTEP_GAME_GORE_HEAD_BURST_DEV_TOOLS_HPP
#define LOCKSTEP_GAME_GORE_HEAD_BURST_DEV_TOOLS_HPP

#include "Gore/HeadBursts.hpp"

#include <Lockstep/App/GameApi.hpp>

namespace game
{
#if LOCKSTEP_DEV_TOOLS
    // Dev (`--dev-head-burst`, host): every head sever bursts, and from tick 300 on the local
    // player turns to the nearest NPC, which is shot dead through the head every 4 s (dead ones
    // are skipped). For checking the effect without a person (with --dev-capture).
    // `--dev-piece-drill` does the same with head severs instead of bursts, and from 1 s after
    // each kill shoots the newest severed piece four times a second (fireAtBodies()).
    // state:  the game state.
    // engine: Engine services.
    void runHeadBurstDrill(lockstep::app::GameState& state, lockstep::app::EngineContext& engine);

    // Draws the head burst controls of the Game dev panel.
    // tuning: the look.
    void headBurstDevSection(HeadBurstTuning& tuning);
#endif
} //namespace game

#endif //LOCKSTEP_GAME_GORE_HEAD_BURST_DEV_TOOLS_HPP
