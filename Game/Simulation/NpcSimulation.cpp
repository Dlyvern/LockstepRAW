#include "Simulation/NpcSimulation.hpp"

#include "GameState.hpp"
#include "Gore/GoreRules.hpp"
#include "Npcs/NpcBrain.hpp"
#include "Npcs/NpcGroups.hpp"
#include "Npcs/NpcRoster.hpp"
#include "Npcs/NpcSenses.hpp"
#include "Player/LocalPlayer.hpp"
#include "Simulation/HostSimulation.hpp"
#include "Simulation/PlayerCombat.hpp"

#include <Lockstep/Net/Replication.hpp>
#include <Lockstep/Net/Session.hpp>
#include <Lockstep/Physics/PhysicsWorld.hpp>
#include <Lockstep/World/Entities.hpp>
#include <Lockstep/World/TimeDomains.hpp>

#include <algorithm>
#include <cmath>
#include <iostream>

namespace game
{
    namespace
    {
        namespace net = lockstep::net;
        namespace world = lockstep::world;

        using lockstep::app::EngineContext;
        using lockstep::app::GameState;

        constexpr math::Vec3 k_up{0.0f, 1.0f, 0.0f};
        constexpr float k_chestShare{0.7f};
        constexpr float k_squadRestartOffset{1.5f};

        std::vector<NpcActor> gatherActors(const GameState& state, const EngineContext& engine)
        {
            std::vector<NpcActor> actors;
            const GameState::Simulation& simulation = state.simulation;

            for (std::size_t index = 0; index < simulation.pawns.size(); ++index)
            {
                const Pawn& pawn = simulation.pawns[index];
                const world::Entity* entity = engine.entities.find(pawn.entity);

                if (entity == nullptr)
                {
                    continue;
                }

                const math::Vec3 foot = pawnFootPosition(entity->transform());
                actors.push_back(NpcActor{
                    .ref = NpcTargetRef{.kind = NpcTargetRef::Kind::Pawn, .index = static_cast<uint32_t>(index)},
                    .isPolice = true,
                    .isPlayer = true,
                    .isDowned = pawn.isDowned,
                    .foot = foot,
                    .eye = eyePosition(entity->transform()),
                    .chest = foot + k_up * (entity->transform().scale.y * k_chestShare),
                    .yaw = pawn.command.yaw,
                    .speed = math::length(pawn.motion.planarVelocity),
                    .controller = pawn.controller});
            }

            for (std::size_t index = 0; index < simulation.npcs.npcs.size(); ++index)
            {
                const Npc& npc = simulation.npcs.npcs[index];
                const world::Entity* entity = engine.entities.find(npc.entity);

                if (entity == nullptr || !isNpcAlive(npc))
                {
                    continue;
                }

                const math::Vec3 foot = pawnFootPosition(entity->transform());
                const float speed = npc.path.empty() ? 0.0f
                    : (npc.isRunning ? state.settings.npcs.runSpeed : state.settings.npcs.walkSpeed);
                actors.push_back(NpcActor{
                    .ref = NpcTargetRef{.kind = NpcTargetRef::Kind::Npc, .index = static_cast<uint32_t>(index)},
                    .isPolice = npc.faction == NpcFaction::Squad,
                    .foot = foot,
                    .eye = eyePosition(entity->transform()),
                    .chest = foot + k_up * (entity->transform().scale.y * k_chestShare),
                    .yaw = npc.yaw,
                    .speed = speed,
                    .controller = npc.controller});
            }

            return actors;
        }

        void voiceLocally(GameState& state, const math::Vec3& speaker, const std::optional<math::Vec3>& listener,
            const std::string& id)
        {
            if (!listener || math::length(speaker - *listener) > state.settings.npcs.calloutRange
                || isSpeaking(state.subtitles))
            {
                return;
            }

            queueLine(state.subtitles, state.dialogue, id);
        }

        void voiceCallouts(GameState& state, EngineContext& engine)
        {
            NpcRoster& roster = state.simulation.npcs;
            const std::optional<math::Vec3> listener = engine.physics.footPosition(
                state.simulation.pawns[k_hostPawn].controller);
            const bool isConnected = engine.session.state() == net::ConnectionState::Connected;

            for (const NpcCallout& callout : roster.callouts)
            {
                const Npc& npc = roster.npcs[callout.speaker];
                std::clog << "Game: NPC '" << npc.name << "' calls out " << callout.id << '\n';

                if (const std::optional<math::Vec3> foot = npcFoot(npc, engine.entities))
                {
                    voiceLocally(state, *foot, listener, callout.id);
                }

                if (!isConnected)
                {
                    continue;
                }

                if (auto sent = engine.session.send(encodeNpcCallout(NpcCalloutMessage{.speaker = npc.entity,
                        .id = callout.id}), net::Delivery::Reliable); !sent)
                {
                    std::clog << "Game: callout not sent: " << sent.error().message << '\n';
                }
            }

            roster.callouts.clear();
        }

        void publishNpcState(GameState& state, EngineContext& engine)
        {
            GameState::Simulation& simulation = state.simulation;

            if (engine.session.state() != net::ConnectionState::Connected)
            {
                simulation.sentNpcs.reset();
                return;
            }

            NpcStateMessage message{};

            for (const Npc& npc : simulation.npcs.npcs)
            {
                message.npcs.push_back(npcView(npc));
            }

            if (simulation.sentNpcs == message)
            {
                return;
            }

            if (auto sent = engine.session.send(encodeNpcState(message), net::Delivery::Reliable); !sent)
            {
                std::clog << "Game: NPC state not sent: " << sent.error().message << '\n';
                return;
            }

            simulation.sentNpcs = std::move(message);
        }

        std::optional<math::Vec3> chestOf(const EngineContext& engine, world::EntityId entity)
        {
            const world::Entity* found = engine.entities.find(entity);

            if (found == nullptr)
            {
                return std::nullopt;
            }

            return pawnFootPosition(found->transform()) + k_up * (found->transform().scale.y * k_chestShare);
        }

        math::Vec3 shotDirection(const math::Vec3& from, const math::Vec3& to)
        {
            const math::Vec3 offset = to - from;
            return math::lengthSquared(offset) > 0.0f ? math::normalize(offset) : math::Vec3{0.0f, 0.0f, -1.0f};
        }

        void applyHits(GameState& state, EngineContext& engine, const std::vector<NpcHit>& hits)
        {
            NpcRoster& roster = state.simulation.npcs;

            for (const NpcHit& hit : hits)
            {
                if (hit.target.kind == NpcTargetRef::Kind::Pawn && hit.target.index < state.simulation.pawns.size())
                {
                    const world::EntityId pawn = state.simulation.pawns[hit.target.index].entity;

                    if (const std::optional<math::Vec3> chest = chestOf(engine, pawn))
                    {
                        recordHitGore(state, pawn, *chest, shotDirection(hit.source, *chest), hit.damage);
                    }

                    hurtPawn(state, engine, hit.target.index, hit.damage, hit.source);
                }
                else if (hit.target.kind == NpcTargetRef::Kind::Npc && hit.target.index < roster.npcs.size()
                    && isNpcAlive(roster.npcs[hit.target.index]))
                {
                    const std::optional<math::Vec3> chest = chestOf(engine, roster.npcs[hit.target.index].entity);
                    const bool killed = damageNpc(roster, hit.target.index, hit.damage, hit.source,
                        NpcTargetRef{.kind = NpcTargetRef::Kind::Npc, .index = static_cast<uint32_t>(hit.shooter)},
                        engine.entities, engine.physics, state.settings.npcs);

                    if (chest)
                    {
                        const lockstep::anim::Limb limb = recordNpcGore(state, NpcGoreHit{.npc = hit.target.index,
                            .point = *chest, .direction = shotDirection(hit.source, *chest), .damage = hit.damage,
                            .killed = killed, .isNpcShot = true,
                            .distance = math::length(*chest - hit.source)});
                        flinchNpc(roster.npcs[hit.target.index], limb, hit.damage, state.settings.npcs);
                    }
                }
            }
        }
    } //namespace

    void buildLevelNavigation(GameState& state, EngineContext& engine, const math::Vec3& minimum,
        const math::Vec3& maximum)
    {
        state.simulation.nav = buildNavGrid(engine.physics, minimum, maximum);
        const NavGrid& grid = *state.simulation.nav;
        std::clog << "Game: navigation grid " << grid.columns[0] << " x " << grid.columns[1] << " columns, "
                  << grid.nodes.size() << " walkable nodes, " << grid.cover.size() << " cover candidates\n";
    }

    void clearLevelNpcs(GameState& state, EngineContext& engine)
    {
        removeNpcs(state.simulation.npcs, engine.entities, engine.physics);
        state.simulation.nav.reset();
        state.simulation.sentNpcs.reset();
    }

    bool shootNpcs(GameState& state, EngineContext& engine, std::size_t pawnIndex, const math::Vec3& origin,
        const math::Vec3& end, lockstep::physics::ControllerId controller, float damageScale)
    {
        NpcRoster& roster = state.simulation.npcs;
        const NpcTargetRef shooter{.kind = NpcTargetRef::Kind::Pawn, .index = static_cast<uint32_t>(pawnIndex)};
        roster.shots.push_back(NpcShotEvent{.origin = origin, .end = end, .isPolice = true, .shooter = shooter});
        const std::optional<std::size_t> struck = npcWithController(roster, controller);

        if (!struck)
        {
            return false;
        }

        if (roster.npcs[*struck].faction == NpcFaction::Squad)
        {
            return true;
        }

        Pawn& pawn = state.simulation.pawns[pawnIndex];
        ++pawn.hitCount;
        ++pawn.hits;

        const int32_t damage = std::max(1, static_cast<int32_t>(std::lround(static_cast<float>(
            state.settings.npcs.playerDamage) * damageScale)));
        const bool killed = damageNpc(roster, *struck, damage, origin, shooter,
            engine.entities, engine.physics, state.settings.npcs);
        const lockstep::anim::Limb limb = recordNpcGore(state, NpcGoreHit{.npc = *struck, .point = end,
            .direction = shotDirection(origin, end), .damage = damage, .killed = killed,
            .shooter = static_cast<uint8_t>(pawnIndex), .distance = math::length(end - origin)});
        flinchNpc(roster.npcs[*struck], limb, damage, state.settings.npcs);

        if (killed)
        {
            ++pawn.killCount;
        }

        return true;
    }

    void beginNpcTick(GameState& state)
    {
        state.simulation.npcs.shots.clear();
    }

    void runNpcHostTick(GameState& state, EngineContext& engine)
    {
        GameState::Simulation& simulation = state.simulation;

        if (simulation.nav && !simulation.npcs.npcs.empty())
        {
            const std::vector<NpcActor> actors = gatherActors(state, engine);
            std::vector<NpcHit> hits;
            const NpcWorld npcWorld{.grid = *simulation.nav,
                .physics = engine.physics,
                .entities = engine.entities,
                .actors = actors,
                .settings = state.settings.npcs,
                .delta = engine.timeDomains.delta(world::TimeDomain::World),
                .hits = hits,
                .shatter = [&state, &engine](lockstep::physics::BodyId body, const math::Vec3& point,
                               const math::Vec3& direction)
                {
                    return state.level && shatterGlass(state.glass, *state.level, engine.physics, body, point, direction);
                }};
            NpcRoster& roster = simulation.npcs;
            updateNpcGroups(roster, npcWorld);

            for (std::size_t index = 0; index < roster.npcs.size(); ++index)
            {
                if (isNpcAlive(roster.npcs[index]))
                {
                    senseNpc(roster, index, npcWorld);
                    thinkNpc(roster, index, npcWorld);
                }
            }

            applyHits(state, engine, hits);
        }

        voiceCallouts(state, engine);
        publishNpcState(state, engine);
        publishGoreEvents(state, engine);
    }

    void restartNpcs(GameState& state, EngineContext& engine)
    {
        GameState::Simulation& simulation = state.simulation;
        NpcRoster& roster = simulation.npcs;
        std::size_t squadIndex = 0;

        for (Npc& npc : roster.npcs)
        {
            math::Vec3 foot = npc.home;

            if (npc.faction == NpcFaction::Squad)
            {
                const std::size_t player = squadIndex % k_playerCount;
                const math::Vec3 base = simulation.checkpoint ? (*simulation.checkpoint)[player]
                                                              : state.settings.spawnPoints[player];
                const float side = squadIndex % 2 == 0 ? -k_squadRestartOffset : k_squadRestartOffset;
                foot = base + math::Vec3{side, 0.0f, k_squadRestartOffset};

                if (simulation.nav)
                {
                    if (const std::optional<uint32_t> node = nearestNavNode(*simulation.nav, foot))
                    {
                        foot = simulation.nav->nodes[*node].position;
                    }
                }

                ++squadIndex;
            }
            else if (!isNpcAlive(npc))
            {
                continue;
            }

            if (auto reset = resetNpc(npc, engine.entities, engine.physics, foot, state.settings.npcs); !reset)
            {
                std::clog << "Game: NPC '" << npc.name << "' not reset: " << reset.error().message << '\n';
            }
        }

        for (std::size_t group = 0; group < roster.groups.size(); ++group)
        {
            NpcGroup& groupState = roster.groups[group];
            const uint32_t members = groupState.members;
            groupState = NpcGroup{.members = members};

            for (const Npc& npc : roster.npcs)
            {
                groupState.losses += npc.group == group && !isNpcAlive(npc) ? 1u : 0u;
            }
        }
    }

    std::vector<lockstep::physics::ControllerId> npcControllers(const GameState& state)
    {
        std::vector<lockstep::physics::ControllerId> controllers;

        for (const Npc& npc : state.simulation.npcs.npcs)
        {
            if (isNpcAlive(npc))
            {
                controllers.push_back(npc.controller);
            }
        }

        return controllers;
    }

    bool receiveNpcMessage(GameState& state, EngineContext& engine, std::span<const std::byte> message)
    {
        if (std::optional<NpcStateMessage> views = decodeNpcState(message))
        {
            state.local.npcViews = std::move(*views);
            return true;
        }

        const std::optional<NpcCalloutMessage> callout = decodeNpcCallout(message);

        if (!callout)
        {
            return false;
        }

        for (const net::MirroredEntity& mirrored : engine.replication.mirrored())
        {
            if (mirrored.hostEntity != callout->speaker)
            {
                continue;
            }

            if (const world::Entity* entity = engine.entities.find(mirrored.entity))
            {
                voiceLocally(state, pawnFootPosition(entity->transform()),
                    engine.physics.footPosition(localController(state, engine)), callout->id);
            }

            break;
        }

        return true;
    }
} //namespace game
