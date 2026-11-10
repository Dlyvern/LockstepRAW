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

        constexpr float k_pieceSpeed{3.0f};
        constexpr float k_weaponSpeed{4.0f};

        struct TargetHit
        {
            NpcLook* look{nullptr};
            uint32_t body{0};
            SeveredPiece* piece{nullptr};
            bool isWeapon{false};
            math::Vec3 point{0.0f, 0.0f, 0.0f};
            float distance{0.0f};
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

        void tracePieces(std::vector<SeveredPiece>& pieces, bool isWeapon, const math::Vec3& origin,
            const math::Vec3& direction, float& reach, std::optional<TargetHit>& nearest)
        {
            for (SeveredPiece& piece : pieces)
            {
                if (const std::optional<float> distance = tracePiece(piece, origin, direction, reach))
                {
                    reach = *distance;
                    nearest = TargetHit{.piece = &piece, .isWeapon = isWeapon, .point = origin + direction * *distance,
                        .distance = *distance};
                }
            }
        }

        std::optional<TargetHit> traceTargets(GameState& state, const math::Vec3& origin, const math::Vec3& direction,
            float reach, bool withWeapons)
        {
            std::optional<TargetHit> nearest;

            for (NpcLook& look : state.npcLooks.looks)
            {
                if (!look.ragdoll || look.model == nullptr)
                {
                    continue;
                }

                if (const std::optional<RagdollRayHit> hit = traceNpcRagdoll(*look.ragdoll, origin, direction, reach))
                {
                    reach = hit->distance;
                    nearest = TargetHit{.look = &look, .body = hit->body, .point = hit->point,
                        .distance = hit->distance};
                }
            }

            tracePieces(state.gore.pieces, false, origin, direction, reach, nearest);

            if (withWeapons)
            {
                tracePieces(state.npcLooks.weapons.weapons, true, origin, direction, reach, nearest);
            }

            return nearest;
        }

        std::optional<CorpseHitReport> reportOf(const EngineContext& engine, const TargetHit& hit,
            const math::Vec3& direction)
        {
            if (hit.isWeapon)
            {
                return std::nullopt;
            }

            if (hit.piece != nullptr)
            {
                const std::optional<world::EntityId> owner = hostEntityOf(engine, hit.piece->owner);

                if (!owner || hit.piece->owner.isNull())
                {
                    return std::nullopt;
                }

                return CorpseHitReport{.target = *owner, .body = k_pieceBody, .limb = hit.piece->limb,
                    .point = hit.point, .direction = direction};
            }

            const std::optional<world::EntityId> target = hostEntityOf(engine, hit.look->entity);

            if (!target || hit.body >= hit.look->ragdoll->joints.size())
            {
                return std::nullopt;
            }

            return CorpseHitReport{.target = *target, .body = static_cast<uint8_t>(hit.body),
                .limb = anim::jointLimb(*hit.look->model, hit.look->ragdoll->joints[hit.body]),
                .point = hit.point, .direction = direction};
        }

        void shootBodies(GameState& state, EngineContext& engine, const math::Vec3& eye, const math::Vec3& direction)
        {
            const float range = state.settings.hitscan.range;
            const std::optional<lockstep::physics::RaycastHit> blocker = engine.physics.raycast(eye, direction, range,
                lockstep::physics::RaycastIgnore{.controller = localController(state, engine)});
            const std::optional<TargetHit> found = traceTargets(state, eye, direction,
                blocker ? blocker->distance : range, true);

            if (!found)
            {
                return;
            }

            if (found->piece != nullptr)
            {
                pushPiece(engine.physics, *found->piece, found->point,
                    direction * (found->isWeapon ? k_weaponSpeed : k_pieceSpeed));
            }
            else
            {
                pushNpcBody(engine.physics, state.npcLooks, *found->look, found->body,
                    direction * state.npcLooks.ragdolls.tuning.hitImpulse, found->point);
            }

            const std::optional<CorpseHitReport> report = reportOf(engine, *found, direction);

            if (!report)
            {
                return;
            }

            state.gore.pending.push_back(GoreEvent{.kind = GoreEventKind::Hit,
                .target = report->target,
                .point = found->point,
                .direction = direction,
                .seed = state.gore.random.state});
            state.gore.random.next();
            queueCorpseHit(state, engine, *report);
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

    void shootBodiesWithNpcShots(GameState& state, EngineContext& engine)
    {
        for (const NpcShotEvent& shot : state.simulation.npcs.shots)
        {
            const math::Vec3 path = shot.end - shot.origin;
            const float reach = math::length(path);

            if (shot.shooter.kind != NpcTargetRef::Kind::Npc || reach <= 0.0f)
            {
                continue;
            }

            const math::Vec3 direction = path / reach;

            if (const std::optional<TargetHit> found = traceTargets(state, shot.origin, direction, reach, false))
            {
                if (std::optional<CorpseHitReport> report = reportOf(engine, *found, direction))
                {
                    report->shooter = k_noShooter;
                    state.simulation.corpseReports.push_back(*report);
                }
            }
        }
    }

    void fireAtBodies(GameState& state, EngineContext& engine, const math::Vec3& eye, const math::Vec3& direction)
    {
        shootBodies(state, engine, eye, direction);
    }
} //namespace game
