#include "Gore/GoreRules.hpp"

#include "GameState.hpp"
#include "Simulation/HostSimulation.hpp"

#include <Lockstep/Net/Session.hpp>
#include <Lockstep/Physics/Ragdoll.hpp>

#include <algorithm>
#include <iostream>

namespace game
{
    namespace
    {
        namespace anim = lockstep::anim;
        namespace net = lockstep::net;

        using lockstep::app::EngineContext;
        using lockstep::app::GameState;

        uint32_t nextSeed(GameState& state)
        {
            uint32_t hash = ++state.simulation.goreSeed * 2654435761u;
            hash ^= hash >> 15;
            return hash * 2246822519u;
        }

        uint8_t damageByte(int32_t damage)
        {
            return static_cast<uint8_t>(std::clamp(damage, 0, 255));
        }

        float chanceRoll(uint32_t seed)
        {
            return static_cast<float>((seed >> 8) & 0xFFFFu) / 65536.0f;
        }

        anim::Limb limbAt(const GameState& state, lockstep::world::EntityId entity, const math::Vec3& point)
        {
            for (const NpcLook& look : state.npcLooks.looks)
            {
                if (look.entity != entity || look.model == nullptr)
                {
                    continue;
                }

                const math::Transform& placement = drawnPlacement(look);
                const math::Vec3 turned = math::conjugate(placement.rotation) * (point - placement.position);
                const math::Vec3 local{turned.x / placement.scale.x, turned.y / placement.scale.y,
                    turned.z / placement.scale.z};
                return anim::nearestLimb(*look.model, look.instance.jointMatrices(), local);
            }

            return anim::Limb::None;
        }

        bool decideSever(const GoreRules& rules, anim::Limb limb, int32_t damage, bool isNpcShot, uint32_t seed)
        {
            const bool isLimb = limb != anim::Limb::None && limb != anim::Limb::Torso;
            const bool isHeadshot = limb == anim::Limb::Head && rules.headshotSevers;
            const bool isHeavy = damage >= rules.severDamage && chanceRoll(seed) < rules.severChance;
            return isLimb && (isHeadshot || isHeavy) && (!isNpcShot || rules.npcSevers);
        }

        GoreEventKind severKind(const GoreRules& rules, anim::Limb limb, int32_t damage, float distance, uint32_t seed)
        {
            if (limb != anim::Limb::Head)
            {
                return GoreEventKind::Sever;
            }

            const float scale = std::clamp(static_cast<float>(damage) / static_cast<float>(std::max(rules.severDamage, 1)),
                0.0f, 1.5f);
            const bool isClose = distance > 0.0f && distance < rules.headBurstRange;
            const float close = isClose ? rules.headBurstBonus * (1.0f - distance / rules.headBurstRange) : 0.0f;
            const float roll = static_cast<float>(((seed * 2246822519u) >> 20) & 0xFFFu) / 4096.0f;
            const bool bursts = rules.headBurstAlways || roll < rules.headBurstChance * scale + close;
            return bursts ? GoreEventKind::HeadBurst : GoreEventKind::Sever;
        }

        const Npc* deadNpc(const GameState& state, lockstep::world::EntityId entity)
        {
            for (const Npc& npc : state.simulation.npcs.npcs)
            {
                if (npc.entity == entity)
                {
                    return npc.mode == NpcMode::Dead ? &npc : nullptr;
                }
            }

            return nullptr;
        }

        void recordCorpseGore(GameState& state, const CorpseHitReport& report, uint8_t shooter)
        {
            const Npc* npc = deadNpc(state, report.target);

            const bool isPiece = report.body == k_pieceBody;

            if (npc == nullptr || (report.body >= lockstep::physics::k_maxRagdollBodies && !isPiece))
            {
                return;
            }

            const uint32_t seed = nextSeed(state);
            const bool severs = !isPiece
                && decideSever(state.settings.gore, report.limb, state.settings.npcs.playerDamage, false, seed);

            const GoreEventKind kind = severs
                ? severKind(state.settings.gore, report.limb, state.settings.npcs.playerDamage, 0.0f, seed)
                : GoreEventKind::Hit;

            if (severs)
            {
                std::clog << "Game: the body of '" << npc->name << "' loses the " << anim::toString(report.limb)
                          << (kind == GoreEventKind::HeadBurst ? " (bursts)" : "") << '\n';
            }

            state.simulation.goreEvents.push_back(GoreEvent{.kind = kind,
                .target = report.target,
                .limb = severs || isPiece ? report.limb : anim::Limb::None,
                .point = report.point,
                .direction = report.direction,
                .seed = seed,
                .body = report.body,
                .shooter = shooter,
                .damage = damageByte(state.settings.npcs.playerDamage)});
        }
    } //namespace

    anim::Limb recordNpcGore(GameState& state, const NpcGoreHit& hit)
    {
        Npc& npc = state.simulation.npcs.npcs[hit.npc];
        const uint32_t seed = nextSeed(state);
        const anim::Limb limb = limbAt(state, npc.entity, hit.point);
        const bool severs = hit.killed && decideSever(state.settings.gore, limb, hit.damage, hit.isNpcShot, seed);

        if (hit.killed)
        {
            npc.hitLimb = limb;
        }

        const GoreEventKind kind = severs ? severKind(state.settings.gore, limb, hit.damage, hit.distance, seed)
                                         : GoreEventKind::Hit;

        if (severs)
        {
            std::clog << "Game: NPC '" << npc.name << "' loses the " << anim::toString(limb)
                      << (kind == GoreEventKind::HeadBurst ? " (bursts)" : "") << '\n';
        }

        state.simulation.goreEvents.push_back(GoreEvent{.kind = kind,
            .target = npc.entity,
            .limb = limb,
            .point = hit.point,
            .direction = hit.direction,
            .seed = seed,
            .shooter = hit.shooter,
            .damage = damageByte(hit.damage)});

        return limb;
    }

    void recordHitGore(GameState& state, lockstep::world::EntityId target, const math::Vec3& point,
        const math::Vec3& direction, int32_t damage)
    {
        state.simulation.goreEvents.push_back(GoreEvent{.kind = GoreEventKind::Hit,
            .target = target,
            .point = point,
            .direction = direction,
            .seed = nextSeed(state),
            .damage = damageByte(damage)});
    }

    void recordSever(GameState& state, lockstep::world::EntityId target, anim::Limb limb, const math::Vec3& point)
    {
        state.simulation.goreEvents.push_back(GoreEvent{.kind = GoreEventKind::Sever,
            .target = target,
            .limb = limb,
            .point = point,
            .direction = math::Vec3{0.0f, 0.3f, -1.0f},
            .seed = nextSeed(state)});
    }

    void queueCorpseHit(GameState& state, EngineContext& engine, const CorpseHitReport& report)
    {
        if (engine.session.role() != net::Role::Client)
        {
            CorpseHitReport own = report;
            own.shooter = static_cast<uint8_t>(k_hostPawn);
            state.simulation.corpseReports.push_back(own);
            return;
        }

        if (engine.session.state() != net::ConnectionState::Connected)
        {
            return;
        }

        if (auto sent = engine.session.send(encodeCorpseHit(report), net::Delivery::Reliable); !sent)
        {
            std::clog << "Game: corpse hit not sent: " << sent.error().message << '\n';
        }
    }

    void applyCorpseHits(GameState& state, EngineContext& engine)
    {
        for (const CorpseHitReport& report : state.simulation.corpseReports)
        {
            recordCorpseGore(state, report, report.shooter);
        }

        state.simulation.corpseReports.clear();

        for (const std::vector<std::byte>& message : engine.session.received())
        {
            if (const std::optional<CorpseHitReport> report = decodeCorpseHit(message))
            {
                recordCorpseGore(state, *report, static_cast<uint8_t>(k_clientPawn));
            }
        }
    }

    void publishGoreEvents(GameState& state, EngineContext& engine)
    {
        std::vector<GoreEvent>& events = state.simulation.goreEvents;

        if (events.empty())
        {
            return;
        }

        if (engine.session.state() == net::ConnectionState::Connected)
        {
            if (auto sent = engine.session.send(encodeGoreEvents(events), net::Delivery::Reliable); !sent)
            {
                std::clog << "Game: gore events not sent: " << sent.error().message << '\n';
            }
        }

        state.gore.pending.insert(state.gore.pending.end(), events.begin(), events.end());
        events.clear();
    }
} //namespace game
