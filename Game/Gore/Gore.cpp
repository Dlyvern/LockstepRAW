#include "Gore/Gore.hpp"

#include "Gore/BodyStainEvents.hpp"

#include "GameState.hpp"
#include "Player/LocalPlayer.hpp"

#include <Lockstep/Net/Replication.hpp>
#include <Lockstep/Net/Session.hpp>
#include <Lockstep/Physics/PhysicsWorld.hpp>
#include <Lockstep/Render/Renderer.hpp>

#include <algorithm>
#include <cmath>
#include <iostream>

namespace game
{
    namespace
    {
        namespace anim = lockstep::anim;
        namespace net = lockstep::net;
        namespace world = lockstep::world;

        using lockstep::app::EngineContext;
        using lockstep::app::GameState;

        constexpr math::Vec3 k_up{0.0f, 1.0f, 0.0f};
        constexpr math::Vec3 k_down{0.0f, -1.0f, 0.0f};
        constexpr float k_twoPi{2.0f * math::k_pi};
        constexpr float k_piecePush{3.0f};
        constexpr float k_exitOffset{0.7f};
        constexpr float k_floorProbe{2.5f};
        constexpr float k_poolDelaySeconds{0.7f};
        constexpr float k_poolProbeLift{0.3f};
        constexpr float k_hitSpeed{3.5f};
        constexpr float k_hitSpread{0.45f};
        constexpr float k_backSpraySpeed{1.6f};
        constexpr uint32_t k_severDropletShare{3};
        constexpr float k_severSpeed{4.5f};
        constexpr float k_severSplatterScale{1.6f};
        constexpr float k_floorSplatterShare{0.6f};
        constexpr float k_pieceLift{0.35f};
        constexpr float k_pieceJitter{0.3f};
        constexpr float k_pieceSpin{10.0f};
        constexpr float k_spurtSeconds{4.0f};
        constexpr float k_spurtInterval{0.32f};
        constexpr uint32_t k_spurtDroplets{5};
        constexpr float k_spurtSpeed{2.6f};
        constexpr float k_dripSpeed{0.5f};
        constexpr float k_dripSpread{0.7f};
        constexpr float k_smearSpacing{0.18f};
        constexpr float k_smearHeight{0.4f};
        constexpr float k_smearSize{0.16f};
        constexpr float k_smearStretch{2.2f};
        constexpr float k_minSplatterSize{0.15f};
        constexpr float k_minPoolSize{0.45f};
        constexpr float k_minDripSize{0.03f};
        constexpr float k_minFloorNormal{0.8f};
        constexpr float k_headshotSplatterScale{1.5f};
        constexpr float k_headshotDroplets{1.5f};

        std::optional<world::EntityId> localEntity(const EngineContext& engine, world::EntityId hostEntity)
        {
            if (engine.session.role() != net::Role::Client)
            {
                return hostEntity;
            }

            for (const net::MirroredEntity& mirrored : engine.replication.mirrored())
            {
                if (mirrored.hostEntity == hostEntity)
                {
                    return mirrored.entity;
                }
            }

            return std::nullopt;
        }

        NpcLook* findLook(NpcLooks& looks, world::EntityId entity)
        {
            const auto found = std::find_if(looks.looks.begin(), looks.looks.end(),
                [entity](const NpcLook& look) { return look.entity == entity; });
            return found != looks.looks.end() ? &*found : nullptr;
        }

        math::Vec3 place(const math::Transform& transform, const math::Vec3& point)
        {
            const math::Vec3 scaled{point.x * transform.scale.x, point.y * transform.scale.y, point.z * transform.scale.z};
            return transform.position + transform.rotation * scaled;
        }

        math::Vec3 jointWorld(const NpcLook& look, uint32_t joint)
        {
            const std::span<const math::Mat4> joints = look.instance.jointMatrices();
            const math::Vec3 local = joint < joints.size() ? math::transformPoint(joints[joint], math::Vec3{0.0f, 0.0f, 0.0f})
                                                           : math::Vec3{0.0f, 0.0f, 0.0f};
            return place(drawnPlacement(look), local);
        }

        void laySplatter(GoreState& gore, const BloodAssets& assets, const lockstep::physics::PhysicsWorld& physics,
            const lockstep::physics::RaycastHit& hit, float size, GoreRandom& random)
        {
            BloodDecal decal = decalOnSurface(hit.point, hit.normal, random.range(0.0f, k_twoPi));
            const auto look = static_cast<std::size_t>(random.next() * static_cast<float>(k_splatterLooks));
            decal.material = assets.splatters[std::min(look, k_splatterLooks - 1)];
            decal.size = size * random.range(0.6f, 1.2f);
            decal.stretch = random.range(1.0f, 1.5f);

            if (fitDecal(physics, decal, k_minSplatterSize))
            {
                addDecal(gore.decals, decal, gore.tuning.maxDecals);
                layWallRun(gore.decals, assets, physics, hit, decal.size, gore.tuning.runs, gore.tuning.maxDecals, random);
            }
        }

        void splatterBehind(GoreState& gore, const BloodAssets& assets, const lockstep::physics::PhysicsWorld& physics,
            const GoreEvent& event, float scale, GoreRandom& random)
        {
            const math::Vec3 exit = event.point + event.direction * k_exitOffset;
            const math::Vec3 sprayed = math::normalize(event.direction + k_down * random.range(0.0f, 0.4f));

            if (const auto hit = physics.raycast(exit, sprayed, gore.tuning.splatterReach); hit && isPaintable(gore.decals, *hit))
            {
                const float reach = std::min(hit->distance / gore.tuning.splatterReach, 1.0f);
                laySplatter(gore, assets, physics, *hit, gore.tuning.splatterSize * scale * (0.6f + 0.6f * reach), random);
            }

            if (const auto floor = physics.raycast(exit, k_down, k_floorProbe); floor && isPaintable(gore.decals, *floor))
            {
                laySplatter(gore, assets, physics, *floor, gore.tuning.splatterSize * scale * k_floorSplatterShare, random);
            }
        }

        bool isHeadshot(const GoreEvent& event)
        {
            return event.kind == GoreEventKind::Hit && event.limb == anim::Limb::Head;
        }

        void showHit(GoreState& gore, const BloodAssets& assets, const lockstep::physics::PhysicsWorld& physics,
            const GoreEvent& event, GoreRandom& random)
        {
            const float head = isHeadshot(event) ? k_headshotDroplets : 1.0f;
            const float strength = bloodStrength(gore.bursts, event.damage) * head;
            const auto droplets = static_cast<uint32_t>(static_cast<float>(gore.tuning.hitDroplets) * strength + 0.5f);
            throwDroplets(gore.spray, SprayBurst{.point = event.point, .direction = event.direction,
                .count = droplets, .speed = k_hitSpeed * std::min(strength, 1.4f), .spread = k_hitSpread},
                gore.tuning.maxDroplets, random);
            throwDroplets(gore.spray, SprayBurst{.point = event.point, .direction = -event.direction,
                .count = droplets / 3, .speed = k_backSpraySpeed, .spread = k_hitSpread},
                gore.tuning.maxDroplets, random);
            splatterBehind(gore, assets, physics, event,
                std::min(strength, 1.5f) * (isHeadshot(event) ? k_headshotSplatterScale : 1.0f), random);
        }

        void addPiece(GoreState& gore, lockstep::physics::PhysicsWorld& physics, SeveredPiece piece)
        {
            while (!gore.pieces.empty() && gore.pieces.size() >= gore.tuning.maxPieces)
            {
                releasePiece(physics, gore.pieces.front());
                gore.pieces.erase(gore.pieces.begin());
            }

            if (gore.tuning.maxPieces == 0)
            {
                releasePiece(physics, piece);
                return;
            }

            gore.pieces.push_back(std::move(piece));
        }

        void showSever(GameState& state, EngineContext& engine, const BloodAssets& assets, const GoreEvent& event,
            GoreRandom& random)
        {
            GoreState& gore = state.gore;
            const std::optional<world::EntityId> entity = localEntity(engine, event.target);
            NpcLook* look = entity ? findLook(state.npcLooks, *entity) : nullptr;

            if (look == nullptr || look->model == nullptr)
            {
                showHit(gore, assets, engine.physics, event, random);
                return;
            }

            const HeldWeapon* weapon = npcHeldWeapon(state.npcLooks, *look->model);
            const anim::PartMask weaponParts = weapon != nullptr ? weapon->parts : 0;
            const SeverSource source{.model = *look->model, .instance = look->instance,
                .transform = drawnPlacement(*look), .gearParts = look->gearParts & ~weaponParts,
                .severed = look->severedLimbs};

            if (!canSever(source, event.limb))
            {
                showHit(gore, assets, engine.physics, event, random);
                return;
            }

            const math::Vec3 jitter{random.range(-1.0f, 1.0f), random.range(-1.0f, 1.0f), random.range(-1.0f, 1.0f)};
            const SeverThrow motion{
                .linear = math::normalize(event.direction + k_up * k_pieceLift + jitter * k_pieceJitter)
                    * (gore.tuning.pieceSpeed * random.range(0.8f, 1.2f)),
                .angular = jitter * k_pieceSpin};
            auto piece = severLimb(engine.physics, source, event.limb, motion);

            if (!piece)
            {
                std::clog << "Game: " << anim::toString(event.limb) << " not severed: " << piece.error().message << '\n';
                showHit(gore, assets, engine.physics, event, random);
                return;
            }

            if (weapon != nullptr && (anim::limbSubtree(event.limb) & anim::limbBit(weapon->limb)) != 0)
            {
                dropNpcWeapon(engine.physics, state.npcLooks, *look, motion.linear, motion.angular);
            }

            const anim::LimbInfo* info = anim::findLimb(*look->model, event.limb);
            const math::Vec3 cut = info != nullptr ? jointWorld(*look, info->rootJoint) : event.point;
            look->severedLimbs = static_cast<anim::LimbMask>(look->severedLimbs | anim::limbBit(event.limb));
            piece.value().owner = *entity;
            piece.value().limb = event.limb;
            addPiece(gore, engine.physics, std::move(piece).value());
            std::clog << "Game: " << anim::toString(event.limb) << " severed\n";
            gore.stumps.push_back(BleedingStump{.entity = *entity, .limb = event.limb});
            throwDroplets(gore.spray, SprayBurst{.point = cut, .direction = math::normalize(event.direction + k_up),
                .count = gore.tuning.hitDroplets * k_severDropletShare, .speed = k_severSpeed, .spread = 0.6f},
                gore.tuning.maxDroplets, random);
            splatterBehind(gore, assets, engine.physics, event, k_severSplatterScale, random);
            splatterBehind(gore, assets, engine.physics, GoreEvent{.point = cut, .direction = event.direction},
                k_severSplatterScale, random);
        }

        void layPools(GameState& state, const EngineContext& engine, const BloodAssets& assets)
        {
            GoreState& gore = state.gore;
            std::erase_if(gore.pooled, [&](world::EntityId entity) { return findLook(state.npcLooks, entity) == nullptr; });

            for (const NpcLook& look : state.npcLooks.looks)
            {
                const bool isPooled = std::find(gore.pooled.begin(), gore.pooled.end(), look.entity) != gore.pooled.end();

                const float deadSeconds = look.ragdoll ? look.ragdoll->seconds : look.deathSeconds;

                if (isPooled || look.model == nullptr || deadSeconds < k_poolDelaySeconds)
                {
                    continue;
                }

                gore.pooled.push_back(look.entity);
                const math::Vec3 hips = jointWorld(look, 0);
                const auto floor = engine.physics.raycast(hips + k_up * k_poolProbeLift, k_down, k_floorProbe);

                if (!floor || !isPaintable(gore.decals, *floor) || floor->normal.y < k_minFloorNormal)
                {
                    continue;
                }

                BloodDecal pool = decalOnSurface(floor->point, floor->normal, gore.random.range(0.0f, k_twoPi));
                pool.material = assets.pool;
                pool.size = gore.tuning.poolSize * gore.random.range(0.8f, 1.2f);
                pool.stretch = gore.random.range(1.0f, 1.4f);
                pool.growSeconds = gore.tuning.poolSeconds;

                if (fitDecal(engine.physics, pool, k_minPoolSize))
                {
                    addDecal(gore.decals, pool, gore.tuning.maxDecals);
                }
            }
        }

        void bleed(GoreState& gore, const math::Vec3& wound, const math::Vec3& outwards, float age, float seconds,
            float& drip)
        {
            drip -= std::max(seconds, 0.0f);

            if (drip > 0.0f)
            {
                return;
            }

            drip = gore.tuning.dripInterval;
            throwDroplets(gore.spray, SprayBurst{.point = wound, .direction = k_down, .count = 1, .speed = k_dripSpeed,
                .spread = k_dripSpread}, gore.tuning.maxDroplets, gore.random);
            const float strength = std::max(0.0f, 1.0f - age / k_spurtSeconds);

            if (strength > 0.0f && gore.random.next() < gore.tuning.dripInterval / k_spurtInterval)
            {
                throwDroplets(gore.spray, SprayBurst{.point = wound, .direction = math::normalize(outwards + k_up * 0.2f),
                    .count = static_cast<uint32_t>(static_cast<float>(k_spurtDroplets) * strength + 0.5f),
                    .speed = k_spurtSpeed * strength, .spread = 0.25f}, gore.tuning.maxDroplets, gore.random);
            }
        }

        void bleedStumps(GameState& state, float seconds)
        {
            GoreState& gore = state.gore;

            std::erase_if(gore.stumps, [&](BleedingStump& stump)
            {
                stump.seconds += seconds;
                const NpcLook* look = findLook(state.npcLooks, stump.entity);
                const anim::LimbInfo* info = look != nullptr && look->model != nullptr
                    ? anim::findLimb(*look->model, stump.limb) : nullptr;

                if (info == nullptr || stump.seconds > gore.tuning.bleedSeconds)
                {
                    return true;
                }

                const uint32_t parent = look->model->skeleton.joints[info->rootJoint].parent;
                const math::Vec3 wound = jointWorld(*look, info->rootJoint);
                const math::Vec3 inner = parent != lockstep::anim::k_noJoint ? jointWorld(*look, parent) : wound - k_down;
                const math::Vec3 outwards = math::lengthSquared(wound - inner) > 0.0f ? math::normalize(wound - inner) : k_up;
                bleed(gore, wound, outwards, stump.seconds, seconds, stump.drip);
                return false;
            });
        }

        void smear(GoreState& gore, const BloodAssets& assets, const lockstep::physics::PhysicsWorld& physics,
            SeveredPiece& piece)
        {
            const math::Vec3 position = piece.current.position;
            const math::Vec3 moved = position - piece.lastSmear;

            if (math::length(moved) < k_smearSpacing)
            {
                return;
            }

            piece.lastSmear = position;
            const auto floor = physics.raycast(position, k_down, k_smearHeight);

            if (!floor || !isPaintable(gore.decals, *floor))
            {
                return;
            }

            BloodDecal decal = decalOnSurface(floor->point, floor->normal, std::atan2(-moved.z, moved.x));
            decal.material = assets.drop;
            decal.size = k_smearSize * gore.random.range(0.7f, 1.2f);
            decal.stretch = k_smearStretch;

            if (fitDecal(physics, decal, k_minDripSize))
            {
                addDecal(gore.decals, decal, gore.tuning.maxDecals);
            }
        }

        void updatePieces(GoreState& gore, const BloodAssets& assets, lockstep::physics::PhysicsWorld& physics,
            float seconds)
        {
            std::erase_if(gore.pieces, [&](SeveredPiece& piece)
            {
                piece.seconds += seconds;

                if (piece.seconds > gore.tuning.pieceSeconds || !physics.isAlive(piece.body))
                {
                    releasePiece(physics, piece);
                    return true;
                }

                trackPiece(piece, physics);
                smear(gore, assets, physics, piece);

                if (piece.seconds < gore.tuning.bleedSeconds)
                {
                    const math::Transform placement = pieceTransform(piece, 1.0f);
                    const math::Vec3 wound = place(placement, piece.cut);
                    const math::Vec3 centre = placement.position + placement.rotation * math::center(piece.bounds);
                    const math::Vec3 outwards = math::lengthSquared(wound - centre) > 0.0f ? math::normalize(wound - centre) : k_up;
                    bleed(gore, wound, outwards, piece.seconds, seconds, piece.drip);
                }

                return false;
            });
        }

        void dropSeveredRagdollBodies(GameState& state, lockstep::physics::PhysicsWorld& physics)
        {
            for (NpcLook& look : state.npcLooks.looks)
            {
                if (!look.ragdoll || look.severedLimbs == 0 || look.model == nullptr)
                {
                    continue;
                }

                for (const anim::LimbInfo& info : look.model->limbs)
                {
                    if ((look.severedLimbs & anim::limbBit(info.limb)) != 0)
                    {
                        severNpcRagdollLimb(physics, *look.model, *look.ragdoll, info.limb);
                    }
                }
            }
        }

        bool pushBody(GameState& state, EngineContext& engine, const GoreEvent& event, GoreEvent& shown)
        {
            const std::optional<world::EntityId> entity = localEntity(engine, event.target);
            NpcLook* look = entity ? findLook(state.npcLooks, *entity) : nullptr;

            if (event.body == k_pieceBody)
            {
                const auto piece = std::find_if(state.gore.pieces.begin(), state.gore.pieces.end(),
                    [&](const SeveredPiece& candidate) { return entity && candidate.owner == *entity
                        && candidate.limb == event.limb; });

                if (piece == state.gore.pieces.end())
                {
                    return false;
                }

                shown.point = piece->current.position;
                pushPiece(engine.physics, *piece, piece->current.position, event.direction * k_piecePush);
                return true;
            }

            if (look == nullptr || !look->ragdoll || event.body >= look->ragdoll->joints.size())
            {
                return false;
            }

            shown.point = ragdollBodyCentre(*look->ragdoll, event.body);
            pushNpcBody(engine.physics, state.npcLooks, *look, event.body,
                event.direction * state.npcLooks.ragdolls.tuning.hitImpulse, std::nullopt);
            return true;
        }

        void showParticleBlood(GameState& state, const GoreEvent& event, bool isBodyHit)
        {
            Effects& effects = state.effects;
            spawnBloodBurst(state.gore.burstEffects, effects.particles ? &*effects.particles : nullptr, effects.atlas,
                state.gore.bursts, BloodBurstHit{.point = event.point,
                    .direction = event.direction,
                    .damage = event.damage,
                    .isHeadshot = isHeadshot(event) || (event.kind == GoreEventKind::Sever && event.limb == anim::Limb::Head),
                    .isCorpse = isBodyHit});
        }

        void flinch(GameState& state, EngineContext& engine, const GoreEvent& event)
        {
            const std::optional<world::EntityId> entity = localEntity(engine, event.target);
            const ReactionHit hit{.point = event.point, .direction = event.direction, .damage = event.damage};

            if (NpcLook* look = entity ? findLook(state.npcLooks, *entity) : nullptr)
            {
                reactToNpcHit(state.npcLooks, *look, hit);
                return;
            }

            const world::Entity* found = entity ? engine.entities.find(*entity) : nullptr;

            if (found != nullptr && found->owner() != world::EntityOwner::Host && state.pawnCharacters)
            {
                reactToPawnHit(*state.pawnCharacters, found->owner(), hit);
            }
        }

        bool isLocalPawn(const EngineContext& engine, const GoreEvent& event, uint8_t local)
        {
            const std::optional<world::EntityId> entity = localEntity(engine, event.target);
            const world::Entity* found = entity ? engine.entities.find(*entity) : nullptr;
            const world::EntityOwner own = local == 0 ? world::EntityOwner::Player1 : world::EntityOwner::Player2;
            return found != nullptr && found->owner() == own;
        }

        void soundHit(GameState& state, EngineContext& engine, const GoreEvent& event, uint8_t local)
        {
            if (event.shooter == local || isLocalPawn(engine, event, local))
            {
                return;
            }

            const bool isHead = event.limb == anim::Limb::Head;
            playFleshHit(state.combatAudio, engine.audio, event.point, bloodStrength(state.gore.bursts, event.damage),
                isHead);
        }

        void applyEvents(GameState& state, EngineContext& engine, const BloodAssets& assets)
        {
            const auto local = static_cast<uint8_t>(localPawnIndex(engine));

            for (const GoreEvent& event : state.gore.pending)
            {
                GoreRandom random = goreRandom(event.seed);
                GoreEvent shown = event;
                const bool isBodyHit = event.body != k_noCorpseBody;
                const bool isOwnBodyHit = isBodyHit && event.shooter == local;
                const bool isPushed = isBodyHit && !isOwnBodyHit && pushBody(state, engine, event, shown);

                if (event.kind == GoreEventKind::HeadBurst)
                {
                    const std::optional<world::EntityId> entity = localEntity(engine, event.target);

                    if (NpcLook* look = entity ? findLook(state.npcLooks, *entity) : nullptr)
                    {
                        showHeadBurst(state, engine, assets, *look, shown, random);
                    }
                    else
                    {
                        showHit(state.gore, assets, engine.physics, shown, random);
                    }
                }
                else if (event.kind == GoreEventKind::Sever)
                {
                    showSever(state, engine, assets, shown, random);
                    showParticleBlood(state, shown, isBodyHit);
                }
                else if (!isBodyHit || isPushed)
                {
                    showHit(state.gore, assets, engine.physics, shown, random);
                    showParticleBlood(state, shown, isBodyHit);
                }

                if (!isBodyHit || isPushed || isOwnBodyHit)
                {
                    stainStruckBody(state, engine, localEntity(engine, event.target), shown);
                }

                if (!isBodyHit)
                {
                    flinch(state, engine, event);
                }

                if (event.kind != GoreEventKind::Hit || !isBodyHit || isPushed)
                {
                    soundHit(state, engine, shown, local);
                }
            }

            state.gore.pending.clear();
        }
    } //namespace

    void presentGore(GameState& state, EngineContext& engine, float alpha, float seconds, const math::Vec3& eye)
    {
        GoreState& gore = state.gore;
        gore.eye = eye;

        if (!gore.isAssetsTried)
        {
            gore.isAssetsTried = true;
            auto assets = createBloodAssets(engine.renderer, engine.files);

            if (assets)
            {
                gore.assets = std::move(assets).value();
            }
            else
            {
                std::clog << "Game: blood not available: " << assets.error().message << '\n';
            }
        }

        if (!gore.assets)
        {
            gore.pending.clear();
            return;
        }

        const BloodAssets& assets = *gore.assets;
        gore.decals.unpaintable = state.level ? state.level->blockers : std::vector<lockstep::physics::BodyId>{};
        applyEvents(state, engine, assets);
        dropSeveredRagdollBodies(state, engine.physics);
        layPools(state, engine, assets);
        bleedStumps(state, seconds);
        updatePieces(gore, assets, engine.physics, seconds);
        presentHeadBursts(state, engine, alpha, seconds);
        updateSpray(gore.spray, engine.physics, DripTarget{.decals = gore.decals, .material = assets.drop,
            .maxDecals = gore.tuning.maxDecals}, seconds, gore.random);
        updateDecals(gore.decals, seconds);

        for (const SeveredPiece& piece : gore.pieces)
        {
            drawPiece(engine.renderer, piece, alpha);
        }

        presentBulletHoles(gore.holes, engine.renderer, BulletHoleFrame{.physics = &engine.physics,
            .level = state.level ? &*state.level : nullptr,
            .impacts = state.combatAudio.impacts,
            .unpaintable = gore.decals.unpaintable,
            .quad = assets.decalQuad,
            .budget = engine.renderer.graphicsQuality().decalBudget});
        drawDecals(engine.renderer, gore.decals, assets.decalQuad,
            assets.hasRuns ? std::span<const lockstep::render::MeshHandle>(assets.runQuads)
                           : std::span<const lockstep::render::MeshHandle>());
        drawSpray(engine.renderer, gore.spray, assets.droplet, assets.dropletLook);
    }

    void clearGore(GoreState& gore, lockstep::physics::PhysicsWorld& physics)
    {
        for (SeveredPiece& piece : gore.pieces)
        {
            releasePiece(physics, piece);
        }

        gore.pieces.clear();
        clearHeadBursts(gore.headBursts, physics);
        gore.stumps.clear();
        gore.pooled.clear();
        gore.decals.decals.clear();
        clearBulletHoles(gore.holes);
        gore.spray.droplets.clear();
        gore.pending.clear();
    }

    bool receiveGoreMessage(GameState& state, std::span<const std::byte> message)
    {
        std::optional<std::vector<GoreEvent>> events = decodeGoreEvents(message);

        if (!events)
        {
            return false;
        }

        state.gore.pending.insert(state.gore.pending.end(), events->begin(), events->end());

        return true;
    }
} //namespace game
