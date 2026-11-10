#include "Npcs/NpcRoster.hpp"

#include "Archetypes.hpp"
#include "Player/Pawns.hpp"

#include <Lockstep/Net/Replication.hpp>
#include <Lockstep/Physics/PhysicsWorld.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>

namespace game
{
    namespace
    {
        namespace physics = lockstep::physics;
        namespace world = lockstep::world;

        constexpr float k_randomScale{1.0f / 16777216.0f};
        constexpr float k_manDownReach{15.0f};
        constexpr float k_attackedReaction{0.2f};
        constexpr float k_decisionStagger{0.8f};
        constexpr float k_minHitDistance{0.1f};
        constexpr std::array<std::string_view, 10> k_mercCallouts{"hospital.gunman.callout.01",
            "hospital.gunman.callout.02", "hospital.gunman.callout.03", "hospital.gunman.callout.04",
            "hospital.gunman.callout.05", "hospital.gunman.callout.06", "hospital.gunman.callout.07",
            "hospital.gunman.callout.08", "hospital.gunman.callout.09", "hospital.gunman.callout.10"};
        constexpr std::array<std::string_view, 10> k_squadCallouts{"hospital.squad.combat.contact",
            "hospital.squad.combat.contact", "hospital.squad.combat.moving", "hospital.squad.combat.covering",
            "hospital.squad.combat.reloading", "", "", "hospital.squad.combat.man_down", "",
            "hospital.squad.combat.clear"};

        bool isUrgent(NpcCalloutKind kind)
        {
            return kind == NpcCalloutKind::Contact || kind == NpcCalloutKind::ContactTwo
                || kind == NpcCalloutKind::ManDown || kind == NpcCalloutKind::FallBack;
        }

        void calloutManDown(NpcRoster& roster, std::size_t fallen, const world::Entities& entities,
            const NpcSettings& settings)
        {
            const std::optional<math::Vec3> where = npcFoot(roster.npcs[fallen], entities);

            if (!where)
            {
                return;
            }

            std::optional<std::size_t> nearest;
            float nearestDistance = k_manDownReach;

            for (std::size_t index = 0; index < roster.npcs.size(); ++index)
            {
                const Npc& other = roster.npcs[index];

                if (index == fallen || !isNpcAlive(other) || other.group != roster.npcs[fallen].group)
                {
                    continue;
                }

                if (const std::optional<math::Vec3> foot = npcFoot(other, entities))
                {
                    const float distance = math::length(*foot - *where);

                    if (distance < nearestDistance)
                    {
                        nearestDistance = distance;
                        nearest = index;
                    }
                }
            }

            if (nearest)
            {
                callOut(roster, *nearest, NpcCalloutKind::ManDown, settings);
            }
        }

        std::optional<float> headingFrom(const math::Vec3& source, const math::Vec3& target)
        {
            const float x = target.x - source.x;
            const float z = target.z - source.z;

            if (x * x + z * z < k_minHitDistance * k_minHitDistance)
            {
                return std::nullopt;
            }

            return std::atan2(x, z);
        }
    } //namespace

    float randomUnit(uint32_t& state)
    {
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;

        return static_cast<float>(state >> 8) * k_randomScale;
    }

    float randomRange(uint32_t& state, float low, float high)
    {
        return low + (high - low) * randomUnit(state);
    }

    const NpcFighterSettings& fighterSettings(const NpcSettings& settings, NpcFaction faction)
    {
        return faction == NpcFaction::Squad ? settings.squad : settings.merc;
    }

    bool isNpcAlive(const Npc& npc)
    {
        return npc.mode != NpcMode::Dead && !npc.controller.isNull();
    }

    lockstep::Status spawnNpc(NpcRoster& roster, world::Entities& entities, lockstep::net::Replication& replication,
        physics::PhysicsWorld& physics, const NpcSpawn& spawn, const NpcSettings& settings)
    {
        auto entity = entities.create({.transform = {.position = spawn.foot},
            .domain = world::TimeDomain::World,
            .owner = world::EntityOwner::Host});

        if (!entity)
        {
            return entity.error();
        }

        const Archetype archetype = spawn.faction == NpcFaction::Squad ? Archetype::Squad : Archetype::Merc;

        if (auto tagged = replication.setArchetype(entity.value(), static_cast<uint16_t>(archetype)); !tagged)
        {
            entities.destroy(entity.value());
            return tagged.error();
        }

        Npc npc{.entity = entity.value(),
            .faction = spawn.faction,
            .group = spawn.group,
            .name = std::string(spawn.name),
            .home = spawn.foot,
            .homeYaw = spawn.yaw};

        if (auto reset = resetNpc(npc, entities, physics, spawn.foot, settings); !reset)
        {
            entities.destroy(entity.value());
            return reset;
        }

        npc.senseTimer = randomRange(roster.random, 0.0f, settings.senseInterval);
        npc.decisionTimer = randomRange(roster.random, 0.0f, k_decisionStagger);
        roster.npcs.push_back(std::move(npc));
        ++roster.groups[std::min<std::size_t>(spawn.group, k_npcGroupCount - 1)].members;

        return {};
    }

    lockstep::Status resetNpc(Npc& npc, world::Entities& entities, physics::PhysicsWorld& physics,
        const math::Vec3& foot, const NpcSettings& settings)
    {
        if (npc.controller.isNull())
        {
            auto controller = physics.createController(pawnController(foot));

            if (!controller)
            {
                return controller.error();
            }

            npc.controller = controller.value();
        }
        else
        {
            physics.setControllerHeight(npc.controller, k_pawnStandHeight, physics::ControllerResize::Force);
            physics.setFootPosition(npc.controller, foot);
        }

        const NpcFighterSettings& fighter = fighterSettings(settings, npc.faction);
        npc.health = fighter.health;
        npc.rounds = fighter.magazine;
        npc.mode = NpcMode::Idle;
        npc.action = NpcAction::Hold;
        npc.actionSeconds = 0.0f;
        npc.yaw = npc.homeYaw;
        npc.isCrouched = false;
        npc.verticalSpeed = 0.0f;
        npc.target = {};
        npc.canSeeTarget = false;
        npc.hasThreat = false;
        npc.reactionSeconds = 0.0f;
        npc.suppression = 0.0f;
        npc.path.clear();
        npc.pathIndex = 0;
        npc.moveReason = NpcMoveReason::None;
        npc.cover.reset();
        npc.failedPeeks = 0;
        npc.burstLeft = 0;
        npc.aimSeconds = 0.0f;
        npc.hitLimb = lockstep::anim::Limb::None;

        if (world::Entity* entity = entities.find(npc.entity))
        {
            placePawnEntity(*entity, foot, npc.yaw, k_pawnStandHeight, 0.0f);
            entity->teleport(entity->transform());
        }

        return {};
    }

    void removeNpcs(NpcRoster& roster, world::Entities& entities, physics::PhysicsWorld& physics)
    {
        for (Npc& npc : roster.npcs)
        {
            entities.destroy(npc.entity);
            physics.destroyController(npc.controller);
        }

        roster.npcs.clear();
        roster.groups = {};
        roster.shots.clear();
        roster.callouts.clear();
    }

    std::optional<std::size_t> npcWithController(const NpcRoster& roster, physics::ControllerId controller)
    {
        if (controller.isNull())
        {
            return std::nullopt;
        }

        for (std::size_t index = 0; index < roster.npcs.size(); ++index)
        {
            if (roster.npcs[index].controller == controller && isNpcAlive(roster.npcs[index]))
            {
                return index;
            }
        }

        return std::nullopt;
    }

    void placeNpcEntity(const Npc& npc, world::Entities& entities, const physics::PhysicsWorld& physics, float height)
    {
        world::Entity* entity = entities.find(npc.entity);
        const std::optional<math::Vec3> foot = physics.footPosition(npc.controller);

        if (entity != nullptr && foot)
        {
            placePawnEntity(*entity, *foot, npc.yaw, height, 0.0f);
        }
    }

    std::optional<math::Vec3> npcEye(const Npc& npc, const world::Entities& entities)
    {
        const world::Entity* entity = entities.find(npc.entity);
        return entity != nullptr ? std::optional<math::Vec3>(eyePosition(entity->transform())) : std::nullopt;
    }

    std::optional<math::Vec3> npcFoot(const Npc& npc, const world::Entities& entities)
    {
        const world::Entity* entity = entities.find(npc.entity);
        return entity != nullptr ? std::optional<math::Vec3>(pawnFootPosition(entity->transform())) : std::nullopt;
    }

    std::string_view calloutId(NpcFaction faction, NpcCalloutKind kind)
    {
        const auto index = static_cast<std::size_t>(kind);
        return faction == NpcFaction::Squad ? k_squadCallouts[index] : k_mercCallouts[index];
    }

    bool callOut(NpcRoster& roster, std::size_t speaker, NpcCalloutKind kind, const NpcSettings& settings)
    {
        if (speaker >= roster.npcs.size() || !isNpcAlive(roster.npcs[speaker]))
        {
            return false;
        }

        const Npc& npc = roster.npcs[speaker];
        const std::string_view id = calloutId(npc.faction, kind);
        NpcGroup& group = roster.groups[std::min<std::size_t>(npc.group, k_npcGroupCount - 1)];

        if (id.empty() || group.calloutSeconds > 0.0f || (!isUrgent(kind) && roster.calloutSeconds > 0.0f))
        {
            return false;
        }

        group.calloutSeconds = settings.calloutCooldown;
        roster.calloutSeconds = settings.calloutCooldown * 0.5f;
        roster.callouts.push_back(NpcCallout{.speaker = speaker, .id = std::string(id)});

        return true;
    }

    bool damageNpc(NpcRoster& roster, std::size_t index, int32_t damage, const math::Vec3& source,
        const NpcTargetRef& attacker, world::Entities& entities, physics::PhysicsWorld& physics,
        const NpcSettings& settings)
    {
        Npc& npc = roster.npcs[index];

        if (!isNpcAlive(npc))
        {
            return false;
        }

        npc.health -= damage;
        npc.suppression += settings.suppressionPerHit;
        npc.threat = source;
        npc.hasThreat = true;
        npc.threatAge = 0.0f;

        if (npc.mode != NpcMode::Combat)
        {
            npc.mode = NpcMode::Combat;
            npc.target = attacker;
            npc.reactionSeconds = std::min(npc.reactionSeconds, k_attackedReaction);
            npc.decisionTimer = 0.0f;
        }

        if (npc.health > 0)
        {
            return false;
        }

        if (const std::optional<math::Vec3> foot = physics.footPosition(npc.controller))
        {
            npc.hitYaw = headingFrom(source, *foot);

            if (world::Entity* entity = entities.find(npc.entity))
            {
                placePawnEntity(*entity, *foot, npc.yaw, npc.isCrouched ? k_npcCrouchHeight : k_pawnStandHeight, 0.0f);
            }
        }

        physics.destroyController(npc.controller);
        npc.controller = {};
        npc.health = 0;
        npc.mode = NpcMode::Dead;
        npc.action = NpcAction::Hold;
        npc.path.clear();
        npc.cover.reset();
        ++roster.groups[std::min<std::size_t>(npc.group, k_npcGroupCount - 1)].losses;
        std::clog << "Game: NPC '" << npc.name << "' died\n";
        calloutManDown(roster, index, entities, settings);

        return true;
    }

    const char* toString(NpcMode mode) noexcept
    {
        switch (mode)
        {
            case NpcMode::Idle:
                return "idle";
            case NpcMode::Alert:
                return "alert";
            case NpcMode::Combat:
                return "combat";
            case NpcMode::Dead:
                return "dead";
        }

        return "unknown";
    }

    const char* toString(NpcAction action) noexcept
    {
        switch (action)
        {
            case NpcAction::Hold:
                return "hold";
            case NpcAction::Move:
                return "move";
            case NpcAction::Cover:
                return "cover";
            case NpcAction::Peek:
                return "peek";
            case NpcAction::Reload:
                return "reload";
        }

        return "unknown";
    }

    const char* toString(NpcMoveReason reason) noexcept
    {
        switch (reason)
        {
            case NpcMoveReason::None:
                return "-";
            case NpcMoveReason::Cover:
                return "to cover";
            case NpcMoveReason::Advance:
                return "advance";
            case NpcMoveReason::Flank:
                return "flank";
            case NpcMoveReason::FallBack:
                return "fall back";
            case NpcMoveReason::Follow:
                return "follow";
        }

        return "unknown";
    }
} //namespace game
