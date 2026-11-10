#ifndef LOCKSTEP_GAME_NPCS_NPC_MOVEMENT_HPP
#define LOCKSTEP_GAME_NPCS_NPC_MOVEMENT_HPP

#include "Npcs/NpcContext.hpp"

namespace game
{
    // How a movement step went.
    // Idle:    no path to follow.
    // Moving:  on the way.
    // Arrived: reached the end of the path this tick.
    // Stuck:   made no progress for a second; the path was dropped.
    enum class NpcMoveResult : uint8_t
    {
        Idle,
        Moving,
        Arrived,
        Stuck
    };

    // Host: plans a path to a goal on the navigation grid and starts walking it.
    // npc:    the NPC; must be alive.
    // world:  the tick's NPC context (grid, entities).
    // goal:   the foot position to reach.
    // reason: why it moves.
    // isRunning: run instead of walk.
    // Returns: true if a path was found (the NPC's action becomes Move).
    bool startNpcMove(Npc& npc, const NpcWorld& world, const math::Vec3& goal, NpcMoveReason reason, bool isRunning);

    // Host: drops the NPC's path.
    // npc: the NPC.
    void stopNpc(Npc& npc);

    // Host: moves an NPC one tick: along its path (skipping waypoints it can walk to straight),
    // falling with gravity, standing up or crouching (its capsule height follows isCrouched where
    // it fits), and places its entity.
    // npc:   the NPC; must be alive.
    // world: the tick's NPC context.
    // Returns: how the path went.
    NpcMoveResult stepNpcMovement(Npc& npc, const NpcWorld& world);

    // Host: turns an NPC towards a point at its turn rate.
    // npc:      the NPC.
    // from:     where the NPC stands.
    // point:    what to face.
    // settings: turn rate.
    // delta:    World delta of this tick.
    // Returns:  the remaining yaw difference, radians (absolute).
    float turnNpcToward(Npc& npc, const math::Vec3& from, const math::Vec3& point, const NpcSettings& settings,
        float delta);

    // Host: turns an NPC towards a yaw at its turn rate.
    // npc:      the NPC.
    // yaw:      the wanted facing.
    // settings: turn rate.
    // delta:    World delta of this tick.
    // Returns:  the remaining yaw difference, radians (absolute).
    float turnNpcToYaw(Npc& npc, float yaw, const NpcSettings& settings, float delta);
} //namespace game

#endif //LOCKSTEP_GAME_NPCS_NPC_MOVEMENT_HPP
