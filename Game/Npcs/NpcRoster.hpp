#ifndef LOCKSTEP_GAME_NPCS_NPC_ROSTER_HPP
#define LOCKSTEP_GAME_NPCS_NPC_ROSTER_HPP

#include "Npcs/Npc.hpp"

#include <Lockstep/Core/Result.hpp>

#include <optional>
#include <string_view>

namespace lockstep::net
{
    class Replication;
} //namespace lockstep::net

namespace lockstep::physics
{
    class PhysicsWorld;
} //namespace lockstep::physics

namespace game
{
    // Where and what to spawn.
    // faction: merc or squad.
    // group:   merc floor group, or k_squadGroup.
    // name:    the marker's name, for logs.
    // foot:    foot position.
    // yaw:     facing.
    struct NpcSpawn
    {
        NpcFaction faction{NpcFaction::Merc};
        uint8_t group{0};
        std::string_view name;
        math::Vec3 foot{0.0f, 0.0f, 0.0f};
        float yaw{0.0f};
    };

    // state:   xorshift state, advanced.
    // Returns: a pseudo-random number in [0, 1).
    [[nodiscard]] float randomUnit(uint32_t& state);

    // state:   xorshift state, advanced.
    // low, high: the range.
    // Returns: a pseudo-random number in [low, high).
    [[nodiscard]] float randomRange(uint32_t& state, float low, float high);

    // settings: NPC tuning.
    // faction:  a faction.
    // Returns:  that faction's fighter tuning.
    [[nodiscard]] const NpcFighterSettings& fighterSettings(const NpcSettings& settings, NpcFaction faction);

    // npc:     an NPC.
    // Returns: true while it is alive.
    [[nodiscard]] bool isNpcAlive(const Npc& npc);

    // Host: spawns an NPC: a World-domain entity tagged Archetype::Merc or Archetype::Squad and a
    // pawn-sized controller, with full health and magazine, idle.
    // roster:      the roster it joins (also counts it in its group).
    // entities:    the entity store.
    // replication: tags the entity.
    // physics:     creates the controller.
    // spawn:       faction, group, place.
    // settings:    NPC tuning.
    // Returns:     success, or the error (nothing is left behind then).
    [[nodiscard]] lockstep::Status spawnNpc(NpcRoster& roster, lockstep::world::Entities& entities,
        lockstep::net::Replication& replication, lockstep::physics::PhysicsWorld& physics, const NpcSpawn& spawn,
        const NpcSettings& settings);

    // Host: puts an NPC (alive or a body) back on its feet at a place, with full health, idle.
    // npc:      the NPC.
    // entities: the entity store.
    // physics:  re-creates or moves the controller.
    // foot:     where it stands.
    // settings: NPC tuning.
    // Returns:  success, or the controller error.
    [[nodiscard]] lockstep::Status resetNpc(Npc& npc, lockstep::world::Entities& entities,
        lockstep::physics::PhysicsWorld& physics, const math::Vec3& foot, const NpcSettings& settings);

    // Host: removes every NPC's entity and controller and empties the roster.
    // roster:   the roster.
    // entities: the entity store.
    // physics:  the physics world.
    void removeNpcs(NpcRoster& roster, lockstep::world::Entities& entities, lockstep::physics::PhysicsWorld& physics);

    // roster:     the roster.
    // controller: a controller a shot hit.
    // Returns:    the index of the living NPC owning it, or nothing.
    [[nodiscard]] std::optional<std::size_t> npcWithController(const NpcRoster& roster,
        lockstep::physics::ControllerId controller);

    // Host: places an NPC's entity at its controller (pawn-like transform: centre, yaw, capsule
    // height as the y scale).
    // npc:      the NPC.
    // entities: the entity store.
    // physics:  the physics world.
    // height:   the capsule height.
    void placeNpcEntity(const Npc& npc, lockstep::world::Entities& entities,
        const lockstep::physics::PhysicsWorld& physics, float height);

    // npc:      an NPC.
    // entities: the entity store.
    // Returns:  its eyes (pawn eye height above its feet), or nothing without an entity.
    [[nodiscard]] std::optional<math::Vec3> npcEye(const Npc& npc, const lockstep::world::Entities& entities);

    // npc:      an NPC.
    // entities: the entity store.
    // Returns:  its feet, or nothing without an entity.
    [[nodiscard]] std::optional<math::Vec3> npcFoot(const Npc& npc, const lockstep::world::Entities& entities);

    // What an NPC calls out. The lines are terse and never explain anything.
    enum class NpcCalloutKind : uint8_t
    {
        Contact,
        ContactTwo,
        Moving,
        Covering,
        Reloading,
        Set,
        FallBack,
        ManDown,
        Hold,
        Clear
    };

    // faction: who speaks.
    // kind:    what they call out.
    // Returns: the dialogue line id (hospital.gunman.callout.* for mercs,
    //          hospital.squad.combat.* for the squad); empty when the faction has no such line.
    [[nodiscard]] std::string_view calloutId(NpcFaction faction, NpcCalloutKind kind);

    // Host: queues a callout if the speaker's group and everyone else may speak now, and starts
    // their cooldowns. Urgent kinds (contact, man down, fall back) only wait for the group.
    // roster:   the roster.
    // speaker:  index of the NPC speaking; must be alive.
    // kind:     what is called out.
    // settings: NPC tuning (cooldown).
    // Returns:  true if it was queued.
    bool callOut(NpcRoster& roster, std::size_t speaker, NpcCalloutKind kind, const NpcSettings& settings);

    // Host: makes a living NPC flinch from a hit that did not kill it: it holds its fire briefly
    // (longer for a head hit) and a heavy or leg hit staggers it (it moves slower). Flinches do
    // not stack beyond their length.
    // npc:      the NPC.
    // limb:     the limb struck (None if unknown).
    // damage:   hit points the hit took.
    // settings: NPC tuning.
    void flinchNpc(Npc& npc, lockstep::anim::Limb limb, int32_t damage, const NpcSettings& settings);

    // Host: applies a hit to an NPC: takes health, suppresses it, makes it fight the attacker;
    // at 0 health it dies (controller removed, the body stays) and a groupmate may call it out.
    // roster:   the roster.
    // index:    the NPC hit; must be alive.
    // damage:   hit points.
    // source:   where the shot came from.
    // attacker: who fired.
    // entities: the entity store.
    // physics:  removes the controller on death.
    // settings: NPC tuning.
    // Returns:  true if the hit killed it.
    bool damageNpc(NpcRoster& roster, std::size_t index, int32_t damage, const math::Vec3& source,
        const NpcTargetRef& attacker, lockstep::world::Entities& entities, lockstep::physics::PhysicsWorld& physics,
        const NpcSettings& settings);
} //namespace game

#endif //LOCKSTEP_GAME_NPCS_NPC_ROSTER_HPP
