#ifndef LOCKSTEP_GAME_NPCS_NPC_FIRING_HPP
#define LOCKSTEP_GAME_NPCS_NPC_FIRING_HPP

#include "Npcs/NpcContext.hpp"

#include <cstddef>

namespace game
{
    // Host: lets an NPC fire bursts at a target it sees. Each shot is a hitscan ray from its eyes
    // towards the target's chest, turned by an aim error that shrinks while it keeps tracking
    // the target (NpcSettings::aimSettleSeconds) and grows with suppression and the target's
    // speed. The first thing on the ray takes the hit: walls and cover stop it, a hit on a
    // hostile actor lands in NpcWorld::hits, a friendly one is wasted. Every shot is recorded in
    // NpcRoster::shots (heard by others).
    // roster: the roster; the NPC's rounds, burst and shot counter change.
    // index:  the NPC; must be alive.
    // world:  the tick's NPC context.
    // target: what it shoots at.
    // eye:    its eyes.
    // Returns: true if it fired this tick.
    bool fireNpcWeapon(NpcRoster& roster, std::size_t index, const NpcWorld& world, const NpcActor& target,
        const math::Vec3& eye);
} //namespace game

#endif //LOCKSTEP_GAME_NPCS_NPC_FIRING_HPP
