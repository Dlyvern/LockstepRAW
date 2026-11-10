#ifndef LOCKSTEP_GAME_MELEE_KICK_RULES_HPP
#define LOCKSTEP_GAME_MELEE_KICK_RULES_HPP

#include <Lockstep/App/GameApi.hpp>

#include <cstddef>

namespace game
{
    // Host, right after a pawn moved for a tick: if its kick strikes on this tick
    // (isKickImpact()), resolves it in the cone in front of the pawn (KickSettings::range and
    // coneCos): a living merc takes KickSettings::damage, staggers and is shoved back (the squad
    // is only shoved), toys fly off, glass panes in reach break (walls only count as struck).
    // Queues a KickEvent for both machines' presentation (GameState::Simulation::kickEvents).
    // state:     the game state.
    // engine:    Engine services.
    // pawnIndex: the pawn that moved.
    void resolvePawnKick(lockstep::app::GameState& state, lockstep::app::EngineContext& engine, std::size_t pawnIndex);
} //namespace game

#endif //LOCKSTEP_GAME_MELEE_KICK_RULES_HPP
