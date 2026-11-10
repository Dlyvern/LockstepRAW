#include "Npcs/NpcGroups.hpp"

#include "Npcs/NpcPositions.hpp"
#include "Npcs/NpcRoster.hpp"

#include <Lockstep/Physics/PhysicsWorld.hpp>

#include <algorithm>
#include <cmath>
#include <iostream>
#include <optional>

namespace game
{
    namespace
    {
        constexpr float k_fallBackStagger{1.5f};
        constexpr float k_flankSide{8.0f};
        constexpr float k_flankWeight{1.5f};
        constexpr float k_flankMinThreat{5.0f};
        constexpr float k_flankMaxThreat{20.0f};
        constexpr float k_flankRadius{16.0f};
        constexpr float k_clearCalmSeconds{8.0f};

        std::optional<std::size_t> firstAlive(const NpcRoster& roster, std::size_t group, std::size_t skip)
        {
            for (std::size_t index = 0; index < roster.npcs.size(); ++index)
            {
                if (index != skip && roster.npcs[index].group == group && isNpcAlive(roster.npcs[index]))
                {
                    return index;
                }
            }

            return std::nullopt;
        }

        void tryFlank(NpcRoster& roster, std::size_t group, const NpcWorld& world)
        {
            std::optional<std::size_t> flanker;

            for (std::size_t index = 0; index < roster.npcs.size(); ++index)
            {
                const Npc& npc = roster.npcs[index];
                const bool isReady = isNpcAlive(npc) && npc.group == group && npc.mode == NpcMode::Combat
                    && npc.hasThreat && npc.reactionSeconds <= 0.0f
                    && (npc.action == NpcAction::Cover || npc.action == NpcAction::Peek);

                if (isReady && (!flanker || npc.failedPeeks > roster.npcs[*flanker].failedPeeks))
                {
                    flanker = index;
                }
            }

            if (!flanker)
            {
                return;
            }

            Npc& npc = roster.npcs[*flanker];
            const std::optional<math::Vec3> foot = world.physics.footPosition(npc.controller);

            if (!foot)
            {
                return;
            }

            const math::Vec3 fromThreat{foot->x - npc.threat.x, 0.0f, foot->z - npc.threat.z};
            const float length = math::length(fromThreat);

            if (length <= 0.0f)
            {
                return;
            }

            const float sign = randomUnit(roster.random) < 0.5f ? 1.0f : -1.0f;
            const math::Vec3 side = math::Vec3{-fromThreat.z, 0.0f, fromThreat.x} * (sign / length);
            CoverQuery query{.from = *foot,
                .threat = npc.threat + k_npcThreatEyeHeight,
                .anchor = npc.threat + side * k_flankSide + fromThreat * (2.0f / length),
                .anchorWeight = k_flankWeight,
                .minThreatDistance = k_flankMinThreat,
                .maxThreatDistance = k_flankMaxThreat,
                .radius = k_flankRadius};

            if (!seekCover(roster, *flanker, world, *foot, NpcMoveReason::Flank, query))
            {
                return;
            }

            std::clog << "Game: NPC '" << npc.name << "' flanks\n";
            callOut(roster, *flanker, NpcCalloutKind::Moving, world.settings);

            if (const std::optional<std::size_t> cover = firstAlive(roster, group, *flanker))
            {
                Npc& mate = roster.npcs[*cover];
                roster.groups[group].calloutSeconds = 0.0f;
                callOut(roster, *cover, NpcCalloutKind::Covering, world.settings);

                if (mate.action == NpcAction::Cover)
                {
                    mate.actionSeconds = 0.0f;
                }
            }
        }
    } //namespace

    void updateNpcGroups(NpcRoster& roster, const NpcWorld& world)
    {
        roster.calloutSeconds = std::max(roster.calloutSeconds - world.delta, 0.0f);

        for (std::size_t group = 0; group < roster.groups.size(); ++group)
        {
            NpcGroup& state = roster.groups[group];
            state.calloutSeconds = std::max(state.calloutSeconds - world.delta, 0.0f);
            state.flankSeconds -= world.delta;
            uint32_t alive = 0;
            uint32_t fighting = 0;
            uint32_t alerted = 0;

            for (Npc& npc : roster.npcs)
            {
                if (npc.group == group && isNpcAlive(npc))
                {
                    ++alive;
                    fighting += npc.mode == NpcMode::Combat ? 1u : 0u;
                    alerted += npc.mode != NpcMode::Idle ? 1u : 0u;
                }
            }

            state.calmSeconds = alerted > 0 ? 0.0f : state.calmSeconds + world.delta;
            state.isEngaged = state.isEngaged || fighting > 0;
            const std::optional<std::size_t> speaker = firstAlive(roster, group, roster.npcs.size());

            if (state.isEngaged && state.calmSeconds > k_clearCalmSeconds && speaker)
            {
                state.isEngaged = false;
                state.calloutSeconds = 0.0f;
                callOut(roster, *speaker, NpcCalloutKind::Clear, world.settings);
            }

            const auto lossesToFall = static_cast<uint32_t>(std::ceil(static_cast<float>(state.members)
                * world.settings.fallBackLosses));
            const bool isMercGroup = group != k_squadGroup;

            if (isMercGroup && state.isEngaged && !state.isFallingBack && state.members >= 2
                && state.losses >= lossesToFall && speaker)
            {
                state.isFallingBack = true;
                state.calloutSeconds = 0.0f;
                callOut(roster, *speaker, NpcCalloutKind::FallBack, world.settings);
                float stagger = 0.0f;

                for (Npc& npc : roster.npcs)
                {
                    if (npc.group == group && isNpcAlive(npc))
                    {
                        npc.hasFallenBack = false;
                        npc.decisionTimer = stagger;
                        stagger += k_fallBackStagger;
                    }
                }

                std::clog << "Game: merc group " << group << " falls back\n";
            }

            if (isMercGroup && state.isEngaged && !state.isFallingBack && fighting >= 2 && state.flankSeconds <= 0.0f)
            {
                state.flankSeconds = randomRange(roster.random, world.settings.flankMin, world.settings.flankMax);
                tryFlank(roster, group, world);
            }
        }
    }
} //namespace game
