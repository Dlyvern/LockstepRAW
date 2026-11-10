#ifndef LOCKSTEP_GAME_NPCS_NPC_FOLLOW_HPP
#define LOCKSTEP_GAME_NPCS_NPC_FOLLOW_HPP

#include "Npcs/NpcContext.hpp"

#include <cstddef>

namespace game
{
    // A squad officer farther than this from the players with no path to them may catch up by
    // appearing behind them, metres.
    inline constexpr float k_npcWarpDistance{10.0f};

    // Host: an idle squad officer follows the nearest player: a spot a few metres behind and to
    // one side of them (the two officers take opposite sides), running when far behind; when the
    // grid has no path (past a mantle, a gap) and it is far behind, it catches up with
    // warpBehind().
    // roster: the roster.
    // index:  the officer; must be alive.
    // foot:   where it stands.
    // world:  the tick's NPC context.
    void thinkFollow(NpcRoster& roster, std::size_t index, const math::Vec3& foot, const NpcWorld& world);

    // Host: moves an NPC to a walkable spot behind or beside a player that no player can see
    // (outside every player's view cone or behind level geometry) and that has a path to the
    // player.
    // npc:    the NPC; must be alive.
    // leader: the player to catch up with.
    // world:  the tick's NPC context.
    // Returns: true if it moved.
    bool warpBehind(Npc& npc, const NpcActor& leader, const NpcWorld& world);
} //namespace game

#endif //LOCKSTEP_GAME_NPCS_NPC_FOLLOW_HPP
