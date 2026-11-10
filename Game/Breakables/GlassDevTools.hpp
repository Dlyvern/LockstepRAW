#ifndef LOCKSTEP_GAME_BREAKABLES_GLASS_DEV_TOOLS_HPP
#define LOCKSTEP_GAME_BREAKABLES_GLASS_DEV_TOOLS_HPP

#include <Lockstep/App/GameApi.hpp>

namespace game
{
#if LOCKSTEP_DEV_TOOLS
    // Host, every tick: with `--dev-break-glass <count>` on the command line, breaks the first
    // `count` intact panes once, 5 seconds into the level (a test without aiming).
    // state:  the game state.
    // engine: Engine services.
    void updateGlassDevBreaks(lockstep::app::GameState& state, lockstep::app::EngineContext& engine);

    // Draws the glass section of the Game dev panel: pane counts and (host) buttons to break the
    // pane nearest the camera or all of them.
    // state:  the game state.
    // engine: Engine services.
    void glassDevSection(lockstep::app::GameState& state, lockstep::app::EngineContext& engine);
#endif
} //namespace game

#endif //LOCKSTEP_GAME_BREAKABLES_GLASS_DEV_TOOLS_HPP
