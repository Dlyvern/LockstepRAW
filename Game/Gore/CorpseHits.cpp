#include "Gore/CorpseHits.hpp"

#include "GameState.hpp"
#include "Gore/GoreRules.hpp"
#include "Player/LocalPlayer.hpp"

#include <Lockstep/Net/Replication.hpp>
#include <Lockstep/Net/Session.hpp>
#include <Lockstep/Physics/PhysicsWorld.hpp>

#include <algorithm>
#include <optional>

namespace game
{
    namespace
    {
        namespace anim = lockstep::anim;
        namespace net = lockstep::net;
        namespace world = lockstep::world;

        using lockstep::app::EngineContext;
        using lockstep::app::GameState;

        constexpr uint8_t k_maxShotsPerFrame{4};

        struct BodyHit
        {
            NpcLook* look{nullptr};
            RagdollRayHit hit{};
        };

        std::optional<world::EntityId> hostEntityOf(const EngineContext& engine, world::EntityId local)
        {
            if (engine.session.role() != net::Role::Client)
            {
                return local;
            }

            for (const net::MirroredEntity& mirrored : engine.replication.mirrored())
            {
                if (mirrored.entity == local)
                {
                    return mirrored.hostEntity;
                }
            }

            return std::nullopt;
        }

        std::optional<BodyHit> traceBodies(NpcLooks& looks, const math::Vec3& eye, const math::Vec3& direction,
            float reach)
        {
            std::optional<BodyHit> nearest;

            for (NpcLook& look : looks.looks)
            {
                if (!look.ragdoll || look.model == nullptr)
                {
                    continue;
                }

                const float limit = nearest ? nearest->hit.distance : reach;

                if (const std::optional<RagdollRayHit> hit = traceNpcRagdoll(*look.ragdoll, eye, direction, limit))
                {
                    nearest = BodyHit{.look = &look, .hit = *hit};
                }
            }

            return nearest;
        }

        void shootBodies(GameState& state, EngineContext& engine, const math::Vec3& eye, const math::Vec3& direction)
        {
            const float range = state.settings.hitscan.range;
            const std::optional<lockstep::physics::RaycastHit> blocker = engine.physics.raycast(eye, direction, range,
                lockstep::physics::RaycastIgnore{.controller = localController(state, engine)});
            const std::optional<BodyHit> found = traceBodies(state.npcLooks, eye, direction,
                blocker ? blocker->distance : range);

            if (!found)
            {
                return;
            }

            NpcLook& look = *found->look;
            const std::optional<world::EntityId> target = hostEntityOf(engine, look.entity);
            const uint32_t body = found->hit.body;

            if (!target || body >= look.ragdoll->joints.size())
            {
                return;
            }

            pushNpcBody(engine.physics, state.npcLooks, look, body,
                direction * state.npcLooks.ragdolls.tuning.hitImpulse, found->hit.point);
            state.gore.pending.push_back(GoreEvent{.kind = GoreEventKind::Hit,
                .target = *target,
                .point = found->hit.point,
                .direction = direction,
                .seed = state.gore.random.state});
            state.gore.random.next();
            queueCorpseHit(state, engine, CorpseHitReport{.target = *target,
                .body = static_cast<uint8_t>(body),
                .limb = anim::jointLimb(*look.model, look.ragdoll->joints[body]),
                .point = found->hit.point,
                .direction = direction});
        }
    } //namespace

    void presentCorpseShots(GameState& state, EngineContext& engine, const math::Vec3& eye, const math::Vec3& direction,
        bool canFire)
    {
        CorpseShots& shots = state.gore.corpseShots;
        const uint8_t current = state.local.command.shotCount;
        const auto fired = static_cast<uint8_t>(current - shots.lastShots);
        const bool hadLast = shots.hasLast;
        shots.lastShots = current;
        shots.hasLast = true;

        if (!hadLast || !canFire || fired == 0)
        {
            return;
        }

        for (uint8_t shot = 0; shot < std::min(fired, k_maxShotsPerFrame); ++shot)
        {
            shootBodies(state, engine, eye, direction);
        }
    }
} //namespace game
