#include "Npcs/NpcSenses.hpp"

#include "Npcs/NpcRoster.hpp"
#include "Player/Pawns.hpp"

#include <Lockstep/Physics/PhysicsWorld.hpp>

#include <algorithm>
#include <cmath>
#include <optional>

namespace game
{
    namespace
    {
        namespace physics = lockstep::physics;

        constexpr int32_t k_maxPassedControllers{4};
        constexpr float k_controllerPass{0.7f};
        constexpr float k_sameFloorHeight{2.5f};
        constexpr float k_downedPenalty{15.0f};
        constexpr float k_currentTargetBonus{3.0f};
        constexpr float k_groupAlertReach{25.0f};
        constexpr float k_switchReaction{0.15f};
        constexpr float k_holdChance{0.5f};
        constexpr uint32_t k_holdMinimumMembers{3};

        bool hasLineOfSight(const physics::PhysicsWorld& world, const math::Vec3& eye, const NpcActor& actor,
            physics::ControllerId self)
        {
            for (const math::Vec3& point : {actor.chest, actor.eye})
            {
                const math::Vec3 offset = point - eye;
                const float distance = math::length(offset);

                if (distance <= 0.0f)
                {
                    return true;
                }

                const std::optional<physics::RaycastHit> hit = world.raycast(eye, offset * (1.0f / distance),
                    distance, physics::RaycastIgnore{.controller = self});

                if (!hit || hit->controller == actor.controller || hit->distance >= distance - 0.05f)
                {
                    return true;
                }
            }

            return false;
        }

        float distanceToSegment(const math::Vec3& point, const math::Vec3& start, const math::Vec3& end)
        {
            const math::Vec3 segment = end - start;
            const float lengthSquared = math::dot(segment, segment);
            const float along = lengthSquared > 0.0f
                ? math::clamp(math::dot(point - start, segment) / lengthSquared, 0.0f, 1.0f) : 0.0f;

            return math::length(start + segment * along - point);
        }

        void alertGroup(NpcRoster& roster, std::size_t index, const NpcWorld& world)
        {
            const Npc& caller = roster.npcs[index];
            const std::optional<math::Vec3> where = npcFoot(caller, world.entities);
            uint32_t alerted = 0;

            for (std::size_t other = 0; other < roster.npcs.size(); ++other)
            {
                Npc& mate = roster.npcs[other];

                if (other == index || !isNpcAlive(mate) || mate.group != caller.group || mate.mode != NpcMode::Idle)
                {
                    continue;
                }

                const std::optional<math::Vec3> foot = npcFoot(mate, world.entities);

                if (!where || !foot || math::length(*foot - *where) > k_groupAlertReach)
                {
                    continue;
                }

                if (mate.faction == NpcFaction::Squad)
                {
                    mate.mode = NpcMode::Combat;
                    mate.target = caller.target;
                    mate.reactionSeconds = randomRange(roster.random, world.settings.reactionMin,
                        world.settings.reactionMax) * 0.5f;
                }
                else
                {
                    mate.mode = NpcMode::Alert;
                }

                mate.threat = caller.threat;
                mate.hasThreat = true;
                mate.threatAge = 0.0f;
                mate.decisionTimer = randomRange(roster.random, 0.1f, 0.8f);
                ++alerted;
            }

            roster.groups[std::min<std::size_t>(caller.group, k_npcGroupCount - 1)].isEngaged = true;

            if (alerted + 1 >= k_holdMinimumMembers && caller.faction == NpcFaction::Merc
                && randomUnit(roster.random) < k_holdChance)
            {
                for (std::size_t other = 0; other < roster.npcs.size(); ++other)
                {
                    if (other != index && roster.npcs[other].group == caller.group && isNpcAlive(roster.npcs[other]))
                    {
                        roster.groups[caller.group].calloutSeconds = 0.0f;
                        roster.calloutSeconds = 0.0f;
                        callOut(roster, other, NpcCalloutKind::Hold, world.settings);
                        break;
                    }
                }
            }
        }

        void hearShots(NpcRoster& roster, Npc& npc, const math::Vec3& eye, const NpcSettings& settings)
        {
            const bool hearsPolice = npc.faction == NpcFaction::Merc;

            for (const NpcShotEvent& shot : roster.shots)
            {
                if (shot.isPolice != hearsPolice)
                {
                    continue;
                }

                const float range = std::abs(shot.origin.y - eye.y) < k_sameFloorHeight ? settings.hearingRange
                                                                                         : settings.hearingOtherFloor;

                if (math::length(shot.origin - eye) <= range)
                {
                    if (npc.mode == NpcMode::Idle)
                    {
                        npc.mode = npc.faction == NpcFaction::Squad ? NpcMode::Combat : NpcMode::Alert;
                        npc.decisionTimer = 0.0f;
                        npc.threat = shot.origin;
                        npc.hasThreat = true;
                        npc.threatAge = 0.0f;
                    }
                    else if (!npc.canSeeTarget)
                    {
                        npc.threat = shot.origin;
                        npc.hasThreat = true;
                        npc.threatAge = 0.0f;
                    }
                }

                if (distanceToSegment(eye, shot.origin, shot.end) <= settings.nearMissDistance)
                {
                    npc.suppression += settings.suppressionPerNearMiss;
                }
            }
        }

        const NpcActor* chooseTarget(const Npc& npc, const math::Vec3& eye, const NpcWorld& world,
            uint32_t& visiblePolice)
        {
            const NpcSettings& settings = world.settings;
            const math::Vec3 forward = lookDirection(npc.yaw, 0.0f);
            const float halfAngle = npc.mode == NpcMode::Idle ? settings.sightHalfAngle
                                                              : std::min(settings.sightHalfAngle * 2.0f, math::k_pi);
            const float minimumCos = std::cos(halfAngle);
            const NpcActor* best = nullptr;
            float bestScore = 0.0f;

            for (const NpcActor& actor : world.actors)
            {
                if (!isHostile(npc, actor))
                {
                    continue;
                }

                const math::Vec3 offset = actor.eye - eye;
                const float distance = math::length(offset);

                if (distance > settings.sightRange)
                {
                    continue;
                }

                const math::Vec3 flat{offset.x, 0.0f, offset.z};
                const float flatLength = math::length(flat);
                const bool isInCone = flatLength <= 0.0f || math::dot(flat * (1.0f / flatLength), forward) >= minimumCos;

                if ((!isInCone && distance > settings.closeSense)
                    || !hasLineOfSight(world.physics, eye, actor, npc.controller))
                {
                    continue;
                }

                visiblePolice += actor.isPolice && actor.isPlayer ? 1u : 0u;
                const float score = distance + (actor.isDowned ? k_downedPenalty : 0.0f)
                    - (actor.ref == npc.target ? k_currentTargetBonus : 0.0f);

                if (best == nullptr || score < bestScore)
                {
                    best = &actor;
                    bestScore = score;
                }
            }

            return best;
        }
    } //namespace

    bool isLevelBlocking(const physics::PhysicsWorld& world, const math::Vec3& from, const math::Vec3& to)
    {
        math::Vec3 origin = from;
        math::Vec3 offset = to - from;
        float distance = math::length(offset);

        if (distance <= 0.0f)
        {
            return false;
        }

        const math::Vec3 direction = offset * (1.0f / distance);

        for (int32_t attempt = 0; attempt <= k_maxPassedControllers && distance > 0.0f; ++attempt)
        {
            const std::optional<physics::RaycastHit> hit = world.raycast(origin, direction, distance);

            if (!hit)
            {
                return false;
            }

            if (hit->controller.isNull())
            {
                return true;
            }

            const float passed = hit->distance + k_controllerPass;
            origin = origin + direction * passed;
            distance -= passed;
        }

        return false;
    }

    void senseNpc(NpcRoster& roster, std::size_t index, const NpcWorld& world)
    {
        Npc& npc = roster.npcs[index];
        const NpcSettings& settings = world.settings;
        const std::optional<math::Vec3> eye = npcEye(npc, world.entities);

        if (!eye)
        {
            return;
        }

        npc.suppression = std::max(npc.suppression - settings.suppressionDecay * world.delta, 0.0f);
        npc.flinchSeconds = std::max(npc.flinchSeconds - world.delta, 0.0f);
        npc.staggerSeconds = std::max(npc.staggerSeconds - world.delta, 0.0f);
        npc.threatAge += world.delta;
        hearShots(roster, npc, *eye, settings);

        if (npc.target.kind != NpcTargetRef::Kind::None && findActor(world.actors, npc.target) == nullptr)
        {
            npc.target = {};
            npc.canSeeTarget = false;
        }

        npc.senseTimer -= world.delta;

        if (npc.senseTimer > 0.0f)
        {
            return;
        }

        npc.senseTimer += settings.senseInterval;
        npc.senseTimer = std::max(npc.senseTimer, 0.0f);
        uint32_t visiblePolice = 0;
        const NpcActor* seen = chooseTarget(npc, *eye, world, visiblePolice);

        if (seen == nullptr)
        {
            npc.canSeeTarget = false;
            return;
        }

        if (npc.mode != NpcMode::Combat)
        {
            const float scale = npc.mode == NpcMode::Alert ? 0.5f : 1.0f;
            npc.reactionSeconds = randomRange(roster.random, settings.reactionMin, settings.reactionMax) * scale;
            npc.mode = NpcMode::Combat;
            npc.decisionTimer = 0.0f;
            npc.target = seen->ref;
            npc.aimSeconds = 0.0f;
            npc.threat = seen->foot;
            npc.hasThreat = true;
            callOut(roster, index, visiblePolice >= 2 ? NpcCalloutKind::ContactTwo : NpcCalloutKind::Contact, settings);
            alertGroup(roster, index, world);
        }
        else if (!(seen->ref == npc.target))
        {
            npc.reactionSeconds = std::max(npc.reactionSeconds, npc.canSeeTarget ? 0.0f : k_switchReaction);
            npc.aimSeconds = 0.0f;
        }

        npc.target = seen->ref;
        npc.canSeeTarget = true;
        npc.threat = seen->foot;
        npc.hasThreat = true;
        npc.threatAge = 0.0f;
    }
} //namespace game
