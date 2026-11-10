#ifndef LOCKSTEP_GAME_SIMULATION_PLAYER_COMBAT_HPP
#define LOCKSTEP_GAME_SIMULATION_PLAYER_COMBAT_HPP

#include <Lockstep/App/GameApi.hpp>
#include <Lockstep/Core/Math.hpp>

#include <cstddef>
#include <cstdint>

namespace game
{
    namespace math = lockstep::math;

    // Host: applies a pawn's new reload presses (R) and advances its reload; issues rounds when
    // the player is armed and has none issued yet (the weapon comes with a full magazine and the
    // issued reserve), and forgets them when unarmed.
    // state:     the game state.
    // engine:    Engine services.
    // pawnIndex: the pawn.
    void updatePawnAmmo(lockstep::app::GameState& state, lockstep::app::EngineContext& engine, std::size_t pawnIndex);

    // Host: takes a round for a pawn's shot (an empty magazine starts a reload instead).
    // state:     the game state.
    // pawnIndex: the shooter.
    // Returns:   true if the shot fires.
    bool takePawnRound(lockstep::app::GameState& state, std::size_t pawnIndex);

    // Host: a hit on a player: health, the HUD's damage counters, downing at 0 health; the
    // squad's failure (both players down, or one bled out) starts the checkpoint restart.
    // state:     the game state.
    // engine:    Engine services.
    // pawnIndex: the player hit.
    // damage:    hit points.
    // source:    where the hit came from.
    void hurtPawn(lockstep::app::GameState& state, lockstep::app::EngineContext& engine, std::size_t pawnIndex,
        int32_t damage, const math::Vec3& source);

    // Host, after movement: advances bleed-outs, revives (the partner holding F within reach of
    // a downed player), starts the restart when every present player is down or one bled out,
    // and runs it once its fade is over: both players respawn at the checkpoint (or the spawn
    // points) with full health and magazines, and the NPCs reset.
    // state:  the game state.
    // engine: Engine services.
    void updateDownedPawns(lockstep::app::GameState& state, lockstep::app::EngineContext& engine);
} //namespace game

#endif //LOCKSTEP_GAME_SIMULATION_PLAYER_COMBAT_HPP
