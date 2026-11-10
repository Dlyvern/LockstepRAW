#ifndef LOCKSTEP_GAME_NPCS_NPC_BRAIN_HPP
#define LOCKSTEP_GAME_NPCS_NPC_BRAIN_HPP

#include "Npcs/NpcContext.hpp"

#include <cstddef>

namespace game
{
    // Host: one NPC's decisions, movement and shooting for one tick (call updateNpcGroups() and
    // senseNpc() first). Idle mercs hold their post; the idle squad follows the nearest player
    // (NpcFollow.hpp). Alert NPCs take cover facing the threat. In combat an NPC reacts after its
    // reaction delay, takes cover (the squad near the players), alternates hiding and peeking
    // (standing up behind low cover, stepping out at corners) to fire bursts, reloads in cover,
    // stays down while suppressed, moves up when it keeps losing its target (or has no cover with
    // a line of fire), falls back when its group does, regroups when the squad falls behind the
    // players, and calms down when the target has been gone for a long time.
    // roster: the roster.
    // index:  the NPC; must be alive.
    // world:  the tick's NPC context.
    void thinkNpc(NpcRoster& roster, std::size_t index, const NpcWorld& world);
} //namespace game

#endif //LOCKSTEP_GAME_NPCS_NPC_BRAIN_HPP
