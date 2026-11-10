#ifndef LOCKSTEP_GAME_NPCS_NPC_CONTEXT_HPP
#define LOCKSTEP_GAME_NPCS_NPC_CONTEXT_HPP

#include "Npcs/NavGrid.hpp"
#include "Npcs/Npc.hpp"

#include <span>
#include <vector>

namespace lockstep::physics
{
    class PhysicsWorld;
} //namespace lockstep::physics

namespace lockstep::world
{
    class Entities;
} //namespace lockstep::world

namespace game
{
    // Someone NPCs can see, hear, follow and shoot, gathered by the host every tick: a living
    // player pawn or a living NPC.
    // ref:        who it is.
    // isPolice:   a player or a squad officer (mercs are hostile to it), else a merc.
    // isPlayer:   a player pawn (the squad follows players).
    // isDowned:   a downed player (a low-priority target).
    // foot, eye, chest: where it stands, sees from and is aimed at.
    // yaw:        which way it faces.
    // speed:      planar speed, m/s (moving targets are harder to hit).
    // controller: its controller, to tell what a ray hit.
    struct NpcActor
    {
        NpcTargetRef ref{};
        bool isPolice{true};
        bool isPlayer{false};
        bool isDowned{false};
        math::Vec3 foot{0.0f, 0.0f, 0.0f};
        math::Vec3 eye{0.0f, 0.0f, 0.0f};
        math::Vec3 chest{0.0f, 0.0f, 0.0f};
        float yaw{0.0f};
        float speed{0.0f};
        lockstep::physics::ControllerId controller{};
    };

    // What NPC code works with during one host tick.
    // grid:     the level's navigation grid.
    // physics:  raycasts and controllers.
    // entities: NPC entities.
    // actors:   everyone NPCs may sense this tick.
    // settings: NPC tuning.
    // delta:    the World domain's delta of this tick.
    // hits:     hits landed by NPC shots this tick, for the host rules to apply (appended to).
    struct NpcWorld
    {
        const NavGrid& grid;
        lockstep::physics::PhysicsWorld& physics;
        lockstep::world::Entities& entities;
        std::span<const NpcActor> actors;
        const NpcSettings& settings;
        float delta{0.0f};
        std::vector<NpcHit>& hits;
    };

    // actors:  the actors of this tick.
    // ref:     who to find.
    // Returns: the actor, or null when it is gone (dead, despawned).
    [[nodiscard]] inline const NpcActor* findActor(std::span<const NpcActor> actors, const NpcTargetRef& ref)
    {
        for (const NpcActor& actor : actors)
        {
            if (actor.ref == ref)
            {
                return &actor;
            }
        }

        return nullptr;
    }

    // npc:     an NPC.
    // actor:   someone.
    // Returns: true if they are on opposite sides.
    [[nodiscard]] inline bool isHostile(const Npc& npc, const NpcActor& actor)
    {
        return actor.isPolice == (npc.faction == NpcFaction::Merc);
    }
} //namespace game

#endif //LOCKSTEP_GAME_NPCS_NPC_CONTEXT_HPP
