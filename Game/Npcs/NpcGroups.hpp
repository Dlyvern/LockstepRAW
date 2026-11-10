#ifndef LOCKSTEP_GAME_NPCS_NPC_GROUPS_HPP
#define LOCKSTEP_GAME_NPCS_NPC_GROUPS_HPP

#include "Npcs/NpcContext.hpp"

namespace game
{
    // Host: what NPC groups decide together, once per tick before their members think: callout
    // and flank timers; "Clear." once a group has been calm (nobody alert or fighting) for a
    // while; falling back in order (members pull back one after another) once a merc group has
    // lost too many; and occasionally one merc of a fighting group flanking to cover off to the
    // side of the threat while a groupmate covers it.
    // roster: the roster.
    // world:  the tick's NPC context.
    void updateNpcGroups(NpcRoster& roster, const NpcWorld& world);
} //namespace game

#endif //LOCKSTEP_GAME_NPCS_NPC_GROUPS_HPP
