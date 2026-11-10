#ifndef LOCKSTEP_GAME_NPCS_NPC_WATCH_HPP
#define LOCKSTEP_GAME_NPCS_NPC_WATCH_HPP

#include "Npcs/NpcContext.hpp"

#include <cstddef>

namespace game
{
    // Host: where a holding squad officer looks, on its own and never along a player's look.
    // It covers a sector: the most open direction around it (long sight lines such as corridors
    // and open doors, away from walls), keeping clear of the other officers' sectors and of the
    // players, slowly sweeping across it and picking again after a while or when it has moved.
    // A threat it gave up on recently (last seen or heard) takes the sector over. It glances at a
    // player for a moment when that player comes close or keeps looking straight at it, then
    // looks away again.
    // roster: the roster.
    // index:  the officer; must be alive and not moving.
    // foot:   where it stands.
    // world:  the tick's NPC context.
    void watchAsSquad(NpcRoster& roster, std::size_t index, const math::Vec3& foot, const NpcWorld& world);
} //namespace game

#endif //LOCKSTEP_GAME_NPCS_NPC_WATCH_HPP
