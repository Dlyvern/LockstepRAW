#include "Npcs/NpcPositions.hpp"

#include "Npcs/NpcMovement.hpp"
#include "Npcs/NpcRoster.hpp"

#include <algorithm>
#include <vector>

namespace game
{
    namespace
    {
        constexpr float k_squadAnchorWeight{2.0f};
        constexpr float k_squadCoverRadius{10.0f};
        constexpr float k_squadCoverReach{4.0f};
        constexpr float k_downedPlayerPenalty{100.0f};

        std::vector<math::Vec3> takenSpots(const NpcRoster& roster, std::size_t index)
        {
            std::vector<math::Vec3> taken;

            if (roster.npcs[index].avoidSpot)
            {
                taken.push_back(*roster.npcs[index].avoidSpot);
            }

            for (std::size_t other = 0; other < roster.npcs.size(); ++other)
            {
                const Npc& npc = roster.npcs[other];

                if (other == index || !isNpcAlive(npc))
                {
                    continue;
                }

                if (npc.cover)
                {
                    taken.push_back(npc.cover->hide);
                }

                if (!npc.path.empty())
                {
                    taken.push_back(npc.path.back());
                }
            }

            return taken;
        }
    } //namespace

    const NpcActor* nearestPlayer(const math::Vec3& foot, const NpcWorld& world)
    {
        const NpcActor* best = nullptr;
        float bestScore = 0.0f;

        for (const NpcActor& actor : world.actors)
        {
            if (!actor.isPlayer)
            {
                continue;
            }

            const float score = math::length(actor.foot - foot) + (actor.isDowned ? k_downedPlayerPenalty : 0.0f);

            if (best == nullptr || score < bestScore)
            {
                best = &actor;
                bestScore = score;
            }
        }

        return best;
    }

    void enterCover(Npc& npc, uint32_t& random, const NpcSettings& settings)
    {
        npc.action = NpcAction::Cover;
        npc.isCrouched = npc.cover && npc.cover->isLow;
        npc.actionSeconds = randomRange(random, settings.coverMin, settings.coverMax);
    }

    CoverQuery coverQuery(const Npc& npc, const math::Vec3& foot, const math::Vec3& threatEye,
        const NpcWorld& world)
    {
        CoverQuery query{.from = foot,
            .threat = threatEye,
            .anchor = foot,
            .radius = world.settings.coverSearchRadius};

        if (npc.faction == NpcFaction::Squad)
        {
            if (const NpcActor* leader = nearestPlayer(foot, world))
            {
                query.anchor = leader->foot;
                query.anchorWeight = k_squadAnchorWeight;
                query.radius = std::max(k_squadCoverRadius, math::length(leader->foot - foot) + k_squadCoverReach);
            }
        }

        return query;
    }

    bool seekCover(NpcRoster& roster, std::size_t index, const NpcWorld& world, const math::Vec3& foot,
        NpcMoveReason reason, const CoverQuery& query)
    {
        Npc& npc = roster.npcs[index];
        const std::vector<math::Vec3> taken = takenSpots(roster, index);
        CoverQuery withTaken = query;
        withTaken.taken = taken;
        const std::optional<CoverSpot> spot = findCover(world.grid, world.physics, withTaken, roster.random);

        if (!spot)
        {
            return false;
        }

        npc.cover = spot;

        if (math::length(spot->hide - foot) < k_npcInCoverDistance)
        {
            stopNpc(npc);
            enterCover(npc, roster.random, world.settings);
            return true;
        }

        if (startNpcMove(npc, world, spot->hide, reason, true))
        {
            return true;
        }


        npc.cover.reset();
        return false;
    }
} //namespace game
