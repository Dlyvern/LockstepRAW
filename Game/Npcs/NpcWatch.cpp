#include "Npcs/NpcWatch.hpp"

#include "Npcs/NpcMovement.hpp"
#include "Npcs/NpcPositions.hpp"
#include "Npcs/NpcRoster.hpp"
#include "Npcs/NpcSenses.hpp"

#include <Lockstep/Physics/PhysicsWorld.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <optional>

namespace game
{
    namespace
    {
        namespace physics = lockstep::physics;

        constexpr std::size_t k_sectorRays{16};
        constexpr float k_sightReach{14.0f};
        constexpr float k_neighbourShare{0.25f};
        constexpr float k_mateSectorPenalty{0.9f};
        constexpr float k_mateReach{12.0f};
        constexpr float k_playerPenalty{0.8f};
        constexpr float k_playerReach{6.0f};
        constexpr float k_keepBonus{0.15f};
        constexpr float k_pickMinSeconds{5.0f};
        constexpr float k_pickMaxSeconds{9.0f};
        constexpr float k_repickDistance{1.5f};
        constexpr float k_scanAmplitude{0.35f};
        constexpr float k_threatScanAmplitude{0.15f};
        constexpr float k_scanPeriodSeconds{7.0f};
        constexpr float k_relaxedTurnShare{0.4f};
        constexpr float k_threatMemorySeconds{25.0f};
        constexpr float k_stareCos{0.985f};
        constexpr float k_stareReach{10.0f};
        constexpr float k_stareSeconds{0.8f};
        constexpr float k_closeReach{1.8f};
        constexpr float k_closeSpeed{0.5f};
        constexpr float k_glanceSeconds{1.6f};
        constexpr float k_glanceCooldownSeconds{7.0f};
        constexpr float k_scanOffsetSeconds{2.9f};

        float yawOf(const math::Vec3& direction)
        {
            return std::atan2(-direction.x, -direction.z);
        }

        math::Vec3 flatDirection(float yaw)
        {
            return math::Vec3{-std::sin(yaw), 0.0f, -std::cos(yaw)};
        }

        float openness(const NpcWorld& world, const Npc& npc, const math::Vec3& eye, float yaw)
        {
            const std::optional<physics::RaycastHit> hit = world.physics.raycast(eye, flatDirection(yaw), k_sightReach,
                physics::RaycastIgnore{.controller = npc.controller});

            return (hit ? hit->distance : k_sightReach) / k_sightReach;
        }

        float sectorPenalty(const NpcRoster& roster, std::size_t index, const math::Vec3& foot, float yaw,
            const NpcWorld& world)
        {
            float penalty = 0.0f;
            const math::Vec3 direction = flatDirection(yaw);

            for (std::size_t other = 0; other < roster.npcs.size(); ++other)
            {
                const Npc& mate = roster.npcs[other];
                const std::optional<math::Vec3> mateFoot = npcFoot(mate, world.entities);

                if (other == index || mate.faction != NpcFaction::Squad || !isNpcAlive(mate) || !mateFoot
                    || math::length(*mateFoot - foot) > k_mateReach)
                {
                    continue;
                }

                penalty += k_mateSectorPenalty * std::max(math::dot(direction, flatDirection(mate.watch.yaw)), 0.0f);
            }

            for (const NpcActor& actor : world.actors)
            {
                const math::Vec3 offset{actor.foot.x - foot.x, 0.0f, actor.foot.z - foot.z};
                const float distance = math::length(offset);

                if (!actor.isPlayer || distance <= 0.0f || distance > k_playerReach)
                {
                    continue;
                }

                const float facing = math::dot(direction, offset * (1.0f / distance));
                penalty += k_playerPenalty * std::max(facing, 0.0f) * std::max(facing, 0.0f);
            }

            return penalty;
        }

        float pickSector(const NpcRoster& roster, std::size_t index, const math::Vec3& foot, const NpcWorld& world)
        {
            const Npc& npc = roster.npcs[index];
            const math::Vec3 eye = foot + k_npcThreatEyeHeight;
            std::array<float, k_sectorRays> open{};

            for (std::size_t ray = 0; ray < k_sectorRays; ++ray)
            {
                open[ray] = openness(world, npc, eye, 2.0f * math::k_pi * static_cast<float>(ray) / k_sectorRays);
            }

            float bestYaw = npc.watch.yaw;
            float bestScore = -1.0e9f;

            for (std::size_t ray = 0; ray < k_sectorRays; ++ray)
            {
                const float yaw = 2.0f * math::k_pi * static_cast<float>(ray) / k_sectorRays;
                const float neighbours = open[(ray + 1) % k_sectorRays] + open[(ray + k_sectorRays - 1) % k_sectorRays];
                const float keep = k_keepBonus
                    * std::max(math::dot(flatDirection(yaw), flatDirection(npc.watch.yaw)), 0.0f);
                const float score = open[ray] * (1.0f - 2.0f * k_neighbourShare) + neighbours * k_neighbourShare + keep
                    - sectorPenalty(roster, index, foot, yaw, world);

                if (score > bestScore)
                {
                    bestScore = score;
                    bestYaw = yaw;
                }
            }

            return std::remainder(bestYaw, 2.0f * math::k_pi);
        }

        bool isStaredAt(const NpcActor& actor, const math::Vec3& eye, const NpcWorld& world)
        {
            const math::Vec3 offset{eye.x - actor.eye.x, 0.0f, eye.z - actor.eye.z};
            const float distance = math::length(offset);

            return distance > 0.0f && distance < k_stareReach
                && math::dot(offset * (1.0f / distance), flatDirection(actor.yaw)) > k_stareCos
                && !isLevelBlocking(world.physics, actor.eye, eye);
        }

        const NpcActor* glanceCause(Npc& npc, const math::Vec3& foot, const NpcWorld& world)
        {
            const math::Vec3 eye = foot + k_npcThreatEyeHeight;
            bool isStared = false;
            const NpcActor* cause = nullptr;

            for (const NpcActor& actor : world.actors)
            {
                if (!actor.isPlayer)
                {
                    continue;
                }

                const float distance = math::length(math::Vec3{actor.foot.x - foot.x, 0.0f, actor.foot.z - foot.z});
                const bool isApproaching = distance < k_closeReach && actor.speed > k_closeSpeed;

                if (isStaredAt(actor, eye, world))
                {
                    isStared = true;
                    cause = npc.watch.stareSeconds >= k_stareSeconds ? &actor : cause;
                }

                cause = isApproaching ? &actor : cause;
            }

            npc.watch.stareSeconds = isStared ? npc.watch.stareSeconds + world.delta : 0.0f;

            return cause;
        }
    } //namespace

    void watchAsSquad(NpcRoster& roster, std::size_t index, const math::Vec3& foot, const NpcWorld& world)
    {
        Npc& npc = roster.npcs[index];
        NpcWatch& watch = npc.watch;
        watch.glanceCooldown = std::max(watch.glanceCooldown - world.delta, 0.0f);
        watch.glanceSeconds = std::max(watch.glanceSeconds - world.delta, 0.0f);
        watch.pickSeconds -= world.delta;
        watch.scanSeconds += world.delta;
        const NpcActor* cause = glanceCause(npc, foot, world);

        if (cause != nullptr && watch.glanceCooldown <= 0.0f)
        {
            watch.glanceAt = cause->ref;
            watch.glanceSeconds = k_glanceSeconds;
            watch.glanceCooldown = k_glanceSeconds + k_glanceCooldownSeconds;
            watch.stareSeconds = 0.0f;
        }

        const NpcActor* glanced = watch.glanceSeconds > 0.0f ? findActor(world.actors, watch.glanceAt) : nullptr;

        if (glanced != nullptr)
        {
            turnNpcToward(npc, foot, glanced->foot, world.settings, world.delta);
            return;
        }

        const bool remembersThreat = npc.hasThreat
            && npc.threatAge < world.settings.giveUpSeconds + k_threatMemorySeconds;

        if (remembersThreat)
        {
            const math::Vec3 toThreat{npc.threat.x - foot.x, 0.0f, npc.threat.z - foot.z};
            watch.yaw = math::length(toThreat) > 0.0f ? yawOf(toThreat) : watch.yaw;
        }
        else if (watch.pickSeconds <= 0.0f || math::length(foot - watch.pickedAt) > k_repickDistance)
        {
            watch.yaw = pickSector(roster, index, foot, world);
            watch.pickSeconds = randomRange(roster.random, k_pickMinSeconds, k_pickMaxSeconds);
            watch.pickedAt = foot;
        }

        const float amplitude = remembersThreat ? k_threatScanAmplitude : k_scanAmplitude;
        const float sweep = amplitude * std::sin(2.0f * math::k_pi
            * (watch.scanSeconds + k_scanOffsetSeconds * static_cast<float>(index)) / k_scanPeriodSeconds);
        turnNpcToYaw(npc, watch.yaw + sweep, world.settings, world.delta * k_relaxedTurnShare);
    }
} //namespace game
