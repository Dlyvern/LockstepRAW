#include "Gore/GoreRules.hpp"

#include "GameState.hpp"
#include "Simulation/HostSimulation.hpp"

#include <Lockstep/Net/Session.hpp>
#include <Lockstep/Physics/Ragdoll.hpp>

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

            if (npc == nullptr || report.body >= lockstep::physics::k_maxRagdollBodies)
            {
                return;
            }

            const uint32_t seed = nextSeed(state);
            const bool severs = decideSever(state.settings.gore, report.limb, state.settings.npcs.playerDamage, false,
                seed);

            if (severs)
            {
                std::clog << "Game: the body of '" << npc->name << "' loses the " << anim::toString(report.limb) << '\n';
            }

            state.simulation.goreEvents.push_back(GoreEvent{.kind = severs ? GoreEventKind::Sever : GoreEventKind::Hit,
                .target = report.target,
                .limb = severs ? report.limb : anim::Limb::None,
                .point = report.point,
                .direction = report.direction,
                .seed = seed,
                .body = report.body,
                .shooter = shooter});
        }
    } //namespace

    void recordNpcGore(GameState& state, const NpcGoreHit& hit)
    {
        Npc& npc = state.simulation.npcs.npcs[hit.npc];
        const uint32_t seed = nextSeed(state);
        const anim::Limb limb = limbAt(state, npc.entity, hit.point);
        const bool severs = hit.killed && decideSever(state.settings.gore, limb, hit.damage, hit.isNpcShot, seed);

        if (hit.killed)
        {
            npc.hitLimb = limb;
        }

        if (severs)
        {
            std::clog << "Game: NPC '" << npc.name << "' loses the " << anim::toString(limb) << '\n';
        }

        state.simulation.goreEvents.push_back(GoreEvent{.kind = severs ? GoreEventKind::Sever : GoreEventKind::Hit,
            .target = npc.entity,
            .limb = severs ? limb : anim::Limb::None,
            .point = hit.point,
            .direction = hit.direction,
            .seed = seed});
    }

    void recordHitGore(GameState& state, lockstep::world::EntityId target, const math::Vec3& point,
        const math::Vec3& direction)
    {
        state.simulation.goreEvents.push_back(GoreEvent{.kind = GoreEventKind::Hit,
            .target = target,
            .point = point,
            .direction = direction,
            .seed = nextSeed(state)});
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
            state.simulation.corpseReports.push_back(report);
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
            recordCorpseGore(state, report, static_cast<uint8_t>(k_hostPawn));
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
