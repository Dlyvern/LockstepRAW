#include "Npcs/NpcFollow.hpp"

#include "Npcs/NpcMovement.hpp"
#include "Npcs/NpcPositions.hpp"
#include "Npcs/NpcRoster.hpp"
#include "Npcs/NpcSenses.hpp"
#include "Player/Pawns.hpp"

#include <Lockstep/Physics/PhysicsWorld.hpp>
#include <Lockstep/World/Entities.hpp>

#include <iostream>
#include <optional>

namespace game
{
    namespace
    {
        constexpr float k_followRetrySeconds{0.7f};
        constexpr float k_followFailedSeconds{2.0f};
        constexpr float k_followRunDistance{6.0f};
        constexpr float k_followSlack{1.0f};
        constexpr float k_followSide{1.2f};
        constexpr float k_warpUnseenNear{1.0f};
        constexpr float k_warpViewCos{0.4f};
        constexpr uint32_t k_warpPathExpansions{4000};

        bool isSeenByPlayers(const math::Vec3& point, const NpcWorld& world)
        {
            for (const NpcActor& actor : world.actors)
            {
                if (!actor.isPlayer)
                {
                    continue;
                }

                const math::Vec3 offset = point - actor.eye;
                const math::Vec3 flat{offset.x, 0.0f, offset.z};
                const float length = math::length(flat);
                const bool isInView = length < k_warpUnseenNear
                    || math::dot(flat * (1.0f / length), lookDirection(actor.yaw, 0.0f)) > k_warpViewCos;

                if (isInView && !isLevelBlocking(world.physics, actor.eye, point + k_npcThreatEyeHeight))
                {
                    return true;
                }
            }

            return false;
        }
    } //namespace

    bool warpBehind(Npc& npc, const NpcActor& leader, const NpcWorld& world)
    {
        const math::Vec3 back = lookDirection(leader.yaw, 0.0f) * -1.0f;
        const math::Vec3 side{-back.z, 0.0f, back.x};

        for (const math::Vec3& offset : {back * 3.0f + side * 1.5f, back * 3.0f - side * 1.5f, back * 5.0f,
                 side * 3.0f, side * -3.0f})
        {
            const std::optional<uint32_t> node = nearestNavNode(world.grid, leader.foot + offset);

            if (!node)
            {
                continue;
            }

            const math::Vec3 spot = world.grid.nodes[*node].position;

            if (isSeenByPlayers(spot, world) || !findNavPath(world.grid, spot, leader.foot, k_warpPathExpansions))
            {
                continue;
            }

            stopNpc(npc);
            npc.action = NpcAction::Hold;
            world.physics.setFootPosition(npc.controller, spot);
            placeNpcEntity(npc, world.entities, world.physics, k_pawnStandHeight);

            if (lockstep::world::Entity* entity = world.entities.find(npc.entity))
            {
                entity->teleport(entity->transform());
            }

            std::clog << "Game: squad officer '" << npc.name << "' caught up with the players\n";
            return true;
        }

        return false;
    }

    void thinkFollow(NpcRoster& roster, std::size_t index, const math::Vec3& foot, const NpcWorld& world)
    {
        Npc& npc = roster.npcs[index];
        npc.isCrouched = false;
        const NpcActor* leader = nearestPlayer(foot, world);

        if (npc.action == NpcAction::Move && npc.path.empty())
        {
            npc.action = NpcAction::Hold;
        }

        if (leader == nullptr)
        {
            return;
        }

        const float distance = math::length(leader->foot - foot);
        npc.decisionTimer -= world.delta;
        const bool isGoalStale = npc.path.empty()
            || math::length(npc.path.back() - leader->foot) > world.settings.squadFollowDistance + k_followSlack * 2.0f;

        if (distance > world.settings.squadFollowDistance + k_followSlack && npc.decisionTimer <= 0.0f && isGoalStale)
        {
            npc.decisionTimer = k_followRetrySeconds;
            const math::Vec3 back = lookDirection(leader->yaw, 0.0f) * -world.settings.squadFollowDistance;
            const math::Vec3 side{-back.z, 0.0f, back.x};
            const float sideSign = index % 2 == 0 ? 1.0f : -1.0f;
            const float sideLength = math::length(side);
            const math::Vec3 sideOffset = sideLength > 0.0f ? side * (sideSign * k_followSide / sideLength) : side;

            if (!startNpcMove(npc, world, leader->foot + back + sideOffset, NpcMoveReason::Follow,
                    distance > k_followRunDistance)
                && !startNpcMove(npc, world, leader->foot, NpcMoveReason::Follow, distance > k_followRunDistance)
                && distance > k_npcWarpDistance)
            {
                npc.decisionTimer = k_followFailedSeconds;
                warpBehind(npc, *leader, world);
            }
        }

        if (npc.action != NpcAction::Move)
        {
            turnNpcToYaw(npc, leader->yaw, world.settings, world.delta);
        }
    }
} //namespace game
