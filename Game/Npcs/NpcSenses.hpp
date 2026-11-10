#ifndef LOCKSTEP_GAME_NPCS_NPC_SENSES_HPP
#define LOCKSTEP_GAME_NPCS_NPC_SENSES_HPP

#include "Npcs/NpcContext.hpp"

#include <Lockstep/Physics/PhysicsTypes.hpp>

#include <cstddef>

namespace game
{
    // Tells whether level geometry stands between two points. Controllers (people) do not block.
    // physics: the physics world.
    // from:    start point.
    // to:      end point.
    // Returns: true if a static or dynamic body is in the way.
    [[nodiscard]] bool isLevelBlocking(const lockstep::physics::PhysicsWorld& physics, const math::Vec3& from,
        const math::Vec3& to);

    // Host: one NPC's senses for one tick. Suppression fades, hostile gunshots are heard (an idle
    // NPC becomes alert, a fighting one learns where the shooter is) and shots passing close add
    // suppression. Every NpcSettings::senseInterval it looks around: hostile actors within its
    // sight range and cone (wider once alert) with a clear line from its eyes are seen; it picks
    // the nearest (downed players last, its current target first), and an unaware NPC starts
    // its reaction delay, enters combat, calls out contact and alerts its group.
    // roster: the roster (callouts, random, groupmates).
    // index:  the NPC; must be alive.
    // world:  the tick's NPC context.
    void senseNpc(NpcRoster& roster, std::size_t index, const NpcWorld& world);
} //namespace game

#endif //LOCKSTEP_GAME_NPCS_NPC_SENSES_HPP
