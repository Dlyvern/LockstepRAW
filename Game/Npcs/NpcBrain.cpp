#include "Npcs/NpcBrain.hpp"

#include "Npcs/NpcFiring.hpp"
#include "Npcs/NpcFollow.hpp"
#include "Npcs/NpcMovement.hpp"
#include "Npcs/NpcPositions.hpp"
#include "Npcs/NpcRoster.hpp"
#include "Npcs/NpcSenses.hpp"
#include "Npcs/NpcTactics.hpp"
#include "Player/Pawns.hpp"

#include <Lockstep/Physics/PhysicsWorld.hpp>

#include <algorithm>
#include <cmath>
#include <optional>
#include <vector>

namespace game
{
    namespace
    {
        constexpr float k_homeDistance{1.0f};
        constexpr float k_aimTolerance{0.25f};
        constexpr float k_setChance{0.3f};
        constexpr float k_reloadShare{0.35f};
        constexpr float k_peekSuppressionScale{1.5f};
        constexpr uint8_t k_peeksBeforeAdvance{2};
        constexpr float k_advanceMinThreat{4.0f};
        constexpr float k_advanceCloser{2.0f};
        constexpr float k_advanceStop{6.0f};
        constexpr float k_alertRetrySeconds{3.0f};
        constexpr float k_holdRetrySeconds{2.5f};
        constexpr float k_leashRetrySeconds{1.5f};
        constexpr float k_fallBackDistance{10.0f};
        constexpr float k_fallBackGain{4.0f};
        constexpr float k_fallBackRadius{18.0f};

        void enterPeek(Npc& npc, uint32_t& random, const NpcSettings& settings)
        {
            npc.action = NpcAction::Peek;
            npc.isCrouched = false;
            npc.isPeekSuccessful = false;
            npc.actionSeconds = randomRange(random, settings.peekMin, settings.peekMax);

            if (npc.cover && !npc.cover->isLow)
            {
                npc.path = {npc.cover->peek};
                npc.pathIndex = 0;
                npc.isRunning = false;
            }
        }

        void thinkIdleMerc(Npc& npc, const math::Vec3& foot, const NpcWorld& world)
        {
            if (npc.action == NpcAction::Move && npc.path.empty())
            {
                npc.action = NpcAction::Hold;
            }

            if (npc.action != NpcAction::Move && math::length(npc.home - foot) > k_homeDistance)
            {
                startNpcMove(npc, world, npc.home, NpcMoveReason::None, false);
            }

            if (npc.action != NpcAction::Move)
            {
                npc.isCrouched = false;
                turnNpcToYaw(npc, npc.homeYaw, world.settings, world.delta);
            }
        }

        void thinkAlert(NpcRoster& roster, std::size_t index, const math::Vec3& foot, const NpcWorld& world)
        {
            Npc& npc = roster.npcs[index];

            if (npc.threatAge > world.settings.giveUpSeconds || !npc.hasThreat)
            {
                npc.mode = NpcMode::Idle;
                npc.cover.reset();
                return;
            }

            if (npc.action == NpcAction::Move && npc.path.empty())
            {
                if (npc.cover && math::length(npc.cover->hide - foot) < k_npcInCoverDistance)
                {
                    enterCover(npc, roster.random, world.settings);
                }
                else
                {
                    npc.cover.reset();
                    npc.action = NpcAction::Hold;
                }
            }

            npc.decisionTimer -= world.delta;

            if (npc.action != NpcAction::Move && !npc.cover && npc.decisionTimer <= 0.0f)
            {
                npc.decisionTimer = k_alertRetrySeconds;
                CoverQuery query = coverQuery(npc, foot, npc.threat + k_npcThreatEyeHeight, world);
                query.needsLineOfFire = false;
                seekCover(roster, index, world, foot, NpcMoveReason::Cover, query);
            }

            if (npc.action != NpcAction::Move)
            {
                npc.isCrouched = npc.cover && npc.cover->isLow;
                turnNpcToward(npc, foot, npc.threat, world.settings, world.delta);
            }
        }

        void finishMove(NpcRoster& roster, std::size_t index, const math::Vec3& foot, const NpcWorld& world)
        {
            Npc& npc = roster.npcs[index];

            if (npc.cover && math::length(npc.cover->hide - foot) < k_npcInCoverDistance)
            {
                const bool hasMovedFar = npc.moveReason == NpcMoveReason::Flank
                    || npc.moveReason == NpcMoveReason::Advance || npc.moveReason == NpcMoveReason::FallBack;
                npc.moveReason = NpcMoveReason::None;
                enterCover(npc, roster.random, world.settings);

                if (hasMovedFar && randomUnit(roster.random) < k_setChance)
                {
                    callOut(roster, index, NpcCalloutKind::Set, world.settings);
                }

                return;
            }

            npc.cover.reset();
            npc.moveReason = NpcMoveReason::None;
            npc.action = NpcAction::Hold;
            npc.decisionTimer = std::min(npc.decisionTimer, 0.5f);
        }

        void advanceToward(Npc& npc, const math::Vec3& foot, const NpcWorld& world)
        {
            if (math::length(npc.threat - foot) <= k_advanceStop || !startNpcMove(npc, world, npc.threat,
                    NpcMoveReason::Advance, false))
            {
                return;
            }

            while (npc.path.size() > 1 && math::length(npc.path.back() - npc.threat) < k_advanceStop)
            {
                npc.path.pop_back();
            }
        }

        void tryReposition(NpcRoster& roster, std::size_t index, const math::Vec3& foot, const math::Vec3& threatEye,
            const NpcWorld& world)
        {
            Npc& npc = roster.npcs[index];
            const NpcGroup& group = roster.groups[std::min<std::size_t>(npc.group, k_npcGroupCount - 1)];
            const float threatDistance = math::length(threatEye - foot);

            if (group.isFallingBack && !npc.hasFallenBack && npc.faction == NpcFaction::Merc)
            {
                npc.hasFallenBack = true;
                const math::Vec3 away = foot - npc.threat;
                const float awayLength = math::length(math::Vec3{away.x, 0.0f, away.z});
                CoverQuery query = coverQuery(npc, foot, threatEye, world);
                query.anchor = awayLength > 0.0f
                    ? foot + math::Vec3{away.x, 0.0f, away.z} * (k_fallBackDistance / awayLength) : foot;
                query.anchorWeight = 1.0f;
                query.minThreatDistance = threatDistance + k_fallBackGain;
                query.radius = k_fallBackRadius;

                if (seekCover(roster, index, world, foot, NpcMoveReason::FallBack, query))
                {
                    return;
                }
            }

            if (npc.faction == NpcFaction::Squad)
            {
                const NpcActor* leader = nearestPlayer(foot, world);

                if (leader != nullptr && math::length(leader->foot - foot) > world.settings.squadLeash)
                {
                    npc.decisionTimer = k_leashRetrySeconds;

                    if (!seekCover(roster, index, world, foot, NpcMoveReason::Follow,
                            coverQuery(npc, foot, threatEye, world))
                        && !startNpcMove(npc, world, leader->foot, NpcMoveReason::Follow, true)
                        && math::length(leader->foot - foot) > k_npcWarpDistance)
                    {
                        warpBehind(npc, *leader, world);
                    }

                    return;
                }
            }

            const bool isLost = npc.failedPeeks >= k_peeksBeforeAdvance && npc.threatAge > world.settings.lostTargetSeconds;

            if (isLost)
            {
                npc.failedPeeks = 0;
                CoverQuery query = coverQuery(npc, foot, threatEye, world);
                query.anchor = npc.threat;
                query.anchorWeight = 1.0f;
                query.minThreatDistance = k_advanceMinThreat;
                query.maxThreatDistance = std::max(threatDistance - k_advanceCloser, k_advanceMinThreat + 1.0f);

                if (seekCover(roster, index, world, foot, NpcMoveReason::Advance, query))
                {
                    callOut(roster, index, NpcCalloutKind::Moving, world.settings);
                    return;
                }
            }

            const bool isCoverLost = npc.cover && npc.action == NpcAction::Cover
                && !isCoverValid(world.physics, *npc.cover, threatEye);

            if (!npc.cover || isCoverLost)
            {
                npc.cover.reset();
                npc.decisionTimer = k_holdRetrySeconds;

                if (!seekCover(roster, index, world, foot, NpcMoveReason::Cover, coverQuery(npc, foot, threatEye, world)))
                {
                    npc.action = NpcAction::Hold;

                    if (!npc.canSeeTarget)
                    {
                        advanceToward(npc, foot, world);
                    }
                }
            }
        }

        void endPeek(NpcRoster& roster, Npc& npc, const NpcWorld& world)
        {
            npc.failedPeeks = npc.isPeekSuccessful ? 0 : static_cast<uint8_t>(npc.failedPeeks + 1);

            if (npc.cover && !npc.cover->isLow)
            {
                npc.path = {npc.cover->hide};
                npc.pathIndex = 0;
            }

            enterCover(npc, roster.random, world.settings);
        }

        void thinkCombat(NpcRoster& roster, std::size_t index, const math::Vec3& foot, const NpcWorld& world)
        {
            Npc& npc = roster.npcs[index];
            const NpcSettings& settings = world.settings;
            const NpcActor* target = findActor(world.actors, npc.target);
            const NpcActor* visible = npc.canSeeTarget ? target : nullptr;

            if (!npc.hasThreat || npc.threatAge > settings.giveUpSeconds)
            {
                npc.mode = NpcMode::Alert;
                npc.target = {};
                npc.threatAge = 0.0f;
                return;
            }

            const math::Vec3 threatEye = visible != nullptr ? visible->eye : npc.threat + k_npcThreatEyeHeight;
            npc.aimSeconds = visible != nullptr ? npc.aimSeconds + world.delta : 0.0f;
            npc.reactionSeconds = std::max(npc.reactionSeconds - world.delta, 0.0f);
            npc.decisionTimer -= world.delta;

            if (npc.action == NpcAction::Move && npc.path.empty())
            {
                finishMove(roster, index, foot, world);
            }

            const bool isBusyMoving = npc.action == NpcAction::Move;

            if (!isBusyMoving && npc.decisionTimer <= 0.0f && npc.reactionSeconds <= 0.0f
                && npc.action != NpcAction::Peek && npc.action != NpcAction::Reload)
            {
                npc.decisionTimer = std::max(npc.decisionTimer, 0.0f) + 1.0f;
                tryReposition(roster, index, foot, threatEye, world);
            }

            const int32_t magazine = fighterSettings(settings, npc.faction).magazine;

            switch (npc.action)
            {
                case NpcAction::Move:
                    if (npc.moveReason == NpcMoveReason::Advance && !npc.cover && visible != nullptr)
                    {
                        stopNpc(npc);
                        npc.action = NpcAction::Hold;
                        npc.decisionTimer = 0.6f;
                    }

                    break;
                case NpcAction::Cover:
                    npc.isCrouched = npc.cover && npc.cover->isLow;
                    npc.actionSeconds -= world.delta;

                    if (npc.suppression > settings.suppressionThreshold)
                    {
                        npc.actionSeconds = std::max(npc.actionSeconds, 0.3f);
                    }

                    if (npc.rounds < static_cast<int32_t>(static_cast<float>(magazine) * k_reloadShare))
                    {
                        npc.action = NpcAction::Reload;
                        npc.actionSeconds = settings.reloadSeconds;
                        callOut(roster, index, NpcCalloutKind::Reloading, settings);
                    }
                    else if (npc.actionSeconds <= 0.0f && npc.reactionSeconds <= 0.0f)
                    {
                        enterPeek(npc, roster.random, settings);
                    }

                    break;
                case NpcAction::Peek:
                    npc.isCrouched = false;
                    npc.isPeekSuccessful = npc.isPeekSuccessful || visible != nullptr;
                    npc.actionSeconds -= world.delta;

                    if (npc.actionSeconds <= 0.0f || npc.rounds <= 0
                        || npc.suppression > settings.suppressionThreshold * k_peekSuppressionScale)
                    {
                        endPeek(roster, npc, world);
                    }

                    break;
                case NpcAction::Reload:
                    npc.isCrouched = npc.cover && npc.cover->isLow;
                    npc.actionSeconds -= world.delta;

                    if (npc.actionSeconds <= 0.0f)
                    {
                        npc.rounds = magazine;
                        enterCover(npc, roster.random, settings);
                    }

                    break;
                case NpcAction::Hold:
                    npc.isCrouched = false;

                    if (npc.rounds <= 0)
                    {
                        npc.action = NpcAction::Reload;
                        npc.actionSeconds = settings.reloadSeconds;
                        callOut(roster, index, NpcCalloutKind::Reloading, settings);
                    }

                    break;
            }

            if (npc.action != NpcAction::Move || !npc.isRunning)
            {
                turnNpcToward(npc, foot, visible != nullptr ? visible->foot : npc.threat, settings, world.delta);
            }
        }

        void fireIfReady(NpcRoster& roster, std::size_t index, const NpcWorld& world)
        {
            Npc& npc = roster.npcs[index];

            if (npc.mode != NpcMode::Combat || !npc.canSeeTarget || npc.reactionSeconds > 0.0f || npc.isCrouched
                || !npc.path.empty() || (npc.action != NpcAction::Peek && npc.action != NpcAction::Hold))
            {
                return;
            }

            const NpcActor* target = findActor(world.actors, npc.target);
            const std::optional<math::Vec3> foot = npcFoot(npc, world.entities);
            const std::optional<math::Vec3> eye = npcEye(npc, world.entities);

            if (target == nullptr || !foot || !eye)
            {
                return;
            }

            if (turnNpcToward(npc, *foot, target->foot, world.settings, 0.0f) <= k_aimTolerance)
            {
                fireNpcWeapon(roster, index, world, *target, *eye);
            }
        }
    } //namespace

    void thinkNpc(NpcRoster& roster, std::size_t index, const NpcWorld& world)
    {
        Npc& npc = roster.npcs[index];
        const std::optional<math::Vec3> foot = world.physics.footPosition(npc.controller);

        if (!foot || world.delta <= 0.0f)
        {
            return;
        }

        switch (npc.mode)
        {
            case NpcMode::Idle:
                if (npc.faction == NpcFaction::Squad)
                {
                    thinkFollow(roster, index, *foot, world);
                }
                else
                {
                    thinkIdleMerc(npc, *foot, world);
                }

                break;
            case NpcMode::Alert:
                thinkAlert(roster, index, *foot, world);
                break;
            case NpcMode::Combat:
                thinkCombat(roster, index, *foot, world);
                break;
            case NpcMode::Dead:
                return;
        }

        stepNpcMovement(npc, world);
        fireIfReady(roster, index, world);
    }
} //namespace game
