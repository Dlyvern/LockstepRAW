#include "Npcs/NpcPresentation.hpp"

#include "Characters/MotionMatchedLocomotion.hpp"
#include "Characters/PawnCharacters.hpp"
#include "Gore/SeveredLimbs.hpp"
#include "Presentation/Visuals.hpp"

#include <Lockstep/Physics/PhysicsWorld.hpp>
#include <Lockstep/Render/Renderer.hpp>

#include <algorithm>
#include <cmath>
#include <iostream>

#if LOCKSTEP_DEV_TOOLS
#include <imgui.h>
#endif

namespace game
{
    namespace
    {
        namespace anim = lockstep::anim;
        namespace render = lockstep::render;

        constexpr math::Vec3 k_upAxis{0.0f, 1.0f, 0.0f};
        constexpr math::Vec3 k_modelRight{1.0f, 0.0f, 0.0f};
        constexpr math::Vec3 k_forward{0.0f, 0.0f, -1.0f};
        constexpr float k_fallSeconds{0.55f};
        constexpr float k_lyingSeconds{100.0f};
        constexpr float k_crouchedHeight{1.5f};
        constexpr float k_crouchSquash{0.72f};
        constexpr float k_crouchLayerFade{0.25f};

        float yawOf(const math::Quat& rotation)
        {
            const math::Vec3 forward = rotation * k_forward;
            return std::atan2(-forward.x, -forward.z);
        }

        ReactionRig& reactionRigFor(NpcLooks& looks, const anim::SkinnedModel& model)
        {
            for (ReactionRig& rig : looks.reactionRigs)
            {
                if (rig.model == &model)
                {
                    return rig;
                }
            }

            const auto unused = std::ranges::find(looks.reactionRigs, nullptr, &ReactionRig::model);
            ReactionRig& rig = unused != looks.reactionRigs.end() ? *unused : looks.reactionRigs.front();
            resolveReactionRig(rig, model);
            return rig;
        }

        NpcLook& lookFor(NpcLooks& looks, lockstep::world::EntityId entity, const NpcView& view)
        {
            for (NpcLook& look : looks.looks)
            {
                if (look.entity == entity)
                {
                    return look;
                }
            }

            looks.looks.push_back(NpcLook{.entity = entity,
                .deathSeconds = view.isDead ? k_lyingSeconds : 0.0f,
                .shotCount = view.shotCount});
            return looks.looks.back();
        }

        NpcRagdoll* freezeCandidate(NpcLooks& looks, const NpcRagdoll* keep, uint32_t& simulating)
        {
            const RagdollTuning& tuning = looks.ragdolls.tuning;
            NpcRagdoll* oldestAsleep = nullptr;
            NpcRagdoll* oldestAwake = nullptr;
            simulating = 0;

            for (NpcLook& look : looks.looks)
            {
                if (!look.ragdoll || !isSimulating(*look.ragdoll) || &*look.ragdoll == keep)
                {
                    continue;
                }

                NpcRagdoll& ragdoll = *look.ragdoll;
                ++simulating;
                NpcRagdoll*& oldest = ragdoll.isAsleep ? oldestAsleep : oldestAwake;
                oldest = oldest == nullptr || ragdoll.order < oldest->order ? &ragdoll : oldest;
            }

            if (oldestAsleep != nullptr)
            {
                return oldestAsleep;
            }

            return oldestAwake != nullptr && oldestAwake->awakeSeconds >= tuning.minSeconds ? oldestAwake : nullptr;
        }

        bool makeRoomForRagdoll(lockstep::physics::PhysicsWorld& physics, NpcLooks& looks, const NpcRagdoll* keep)
        {
            const uint32_t cap = std::max(looks.ragdolls.tuning.maxSimulating, 1u);

            while (true)
            {
                uint32_t simulating = 0;
                NpcRagdoll* candidate = freezeCandidate(looks, keep, simulating);

                if (simulating < cap)
                {
                    return true;
                }

                if (candidate == nullptr)
                {
                    return false;
                }

                freezeNpcRagdoll(physics, *candidate);
            }
        }

        void dropWeaponOnDeath(lockstep::physics::PhysicsWorld& physics, NpcLooks& looks, NpcLook& look,
            const math::Vec3& push)
        {
            const WeaponDropTuning& tuning = looks.weapons.tuning;
            const float side = (look.entity.index % 2 == 0) ? 1.0f : -1.0f;
            dropNpcWeapon(physics, looks, look, push * tuning.speed + k_upAxis * tuning.lift,
                math::Vec3{side * tuning.spin, 0.0f, tuning.spin * 0.5f});
        }

        bool drawRagdoll(render::Renderer& renderer, lockstep::physics::PhysicsWorld& physics, NpcLooks& looks,
            NpcLook& look, const anim::SkinnedModel& model, const NpcView& view, const math::Vec3& push,
            const math::Transform& placement, float alpha, float seconds, anim::PartMask parts)
        {
            NpcRagdolls& ragdolls = looks.ragdolls;

            if (!look.ragdoll && !look.isRagdollTried && ragdolls.tuning.isEnabled)
            {
                look.isRagdollTried = true;

                if (makeRoomForRagdoll(physics, looks, nullptr))
                {
                    look.ragdoll = startNpcRagdoll(physics, ragdolls, model, look.instance, placement,
                        RagdollPush{.direction = push, .limb = view.hitLimb, .impulse = ragdolls.tuning.shotImpulse});
                }

                if (look.ragdoll)
                {
                    look.instance.beginDriven(ragdolls.tuning.blendSeconds);
                }
            }

            if (!look.ragdoll)
            {
                return false;
            }

            const auto driven = updateNpcRagdoll(physics, *look.ragdoll, alpha, seconds);
            look.instance.updateDriven(model, look.ragdoll->placement, driven, seconds);
            look.instance.submit(renderer, model, look.ragdoll->placement, render::k_defaultRenderLayer, parts,
                look.stains.surface);
            return true;
        }

        void dropRagdoll(lockstep::physics::PhysicsWorld& physics, NpcLook& look)
        {
            if (look.ragdoll)
            {
                freezeNpcRagdoll(physics, *look.ragdoll);
                look.ragdoll.reset();
            }

            look.isRagdollTried = false;
        }
    } //namespace

    void beginNpcLooks(NpcLooks& looks)
    {
        for (NpcLook& look : looks.looks)
        {
            look.isDrawn = false;
        }
    }

    void drawNpc(render::Renderer& renderer, lockstep::physics::PhysicsWorld& physics, NpcLooks& looks,
        const PawnCharacters* characters, const Visuals& visuals, Archetype archetype, lockstep::world::EntityId entity,
        const math::Transform& transform, const NpcView& view, float alpha, float seconds)
    {
        NpcLook& look = lookFor(looks, entity, view);
        look.isDrawn = true;
        look.shotCount = view.shotCount;
        const bool isMerc = archetype == Archetype::Merc;

        const std::optional<NpcArmedView> armed = characters != nullptr ? npcArmedModel(*characters, view.body)
                                                                         : std::nullopt;

        if (!armed)
        {
            renderer.submit(visuals.cube, isMerc ? visuals.enemy : visuals.player1, transform);
            return;
        }

        const anim::SkinnedModel* model = &armed->body->skinned();
        look.model = model;
        look.gearParts = gearMask(armed->body->gearMasks, look.isHeadBurst ? GearLook::Balaclava : view.gearLook);
        const math::Vec3 foot = transform.position - k_upAxis * (0.5f * transform.scale.y);
        const float yaw = yawOf(transform.rotation);
        const math::Quat facing = math::angleAxis(yaw + math::k_pi, k_upAxis);

        if (view.isDead)
        {
            resetHitReaction(look.reaction);

            if (look.instance.jointMatrices().size() != model->skeleton.joints.size())
            {
                look.instance.update(*model, 0.0f);
            }

            const math::Vec3 push = hitDirection(view).value_or(-(transform.rotation * k_forward));
            const math::Transform placement{.position = foot, .rotation = facing};

            if (!look.isWeaponDropped)
            {
                look.modelTransform = look.deathSeconds >= k_lyingSeconds ? placement : look.modelTransform;
                dropWeaponOnDeath(physics, looks, look, push);
            }

            const anim::PartMask parts = npcBodyParts(looks, look);

            if (drawRagdoll(renderer, physics, looks, look, *model, view, push, placement, alpha, seconds, parts))
            {
                return;
            }

            const float before = look.deathSeconds;
            look.deathSeconds += seconds;
            const float fall = std::min(look.deathSeconds / k_fallSeconds, 1.0f);

            if (before < k_fallSeconds)
            {
                look.instance.update(*model, seconds);
            }

            look.modelTransform = math::Transform{.position = foot,
                .rotation = facing * math::angleAxis(-0.5f * math::k_pi * fall * fall, k_modelRight)};
            look.instance.submit(renderer, *model, look.modelTransform, render::k_defaultRenderLayer, parts,
                look.stains.surface);
            return;
        }

        dropRagdoll(physics, look);
        look.deathSeconds = 0.0f;
        look.isWeaponDropped = false;
        const anim::PartMask parts = npcBodyParts(looks, look);
        const bool isCrouched = transform.scale.y < k_crouchedHeight || looks.isCrouchForced;
        const LocomotionTuning& tuning = characters->locomotionTuning;
        const MotionSets* sets = tuning.isMotionMatched && armed->body->motionSets ? &*armed->body->motionSets : nullptr;
        const bool isMatched = sets != nullptr
            && updateMotionMatchedLocomotion(look.locomotion, *sets, motionStance(isCrouched, view.isAlert, false),
                look.instance.animator(), foot, yaw, seconds, tuning);
        const bool isCrouchClip = isCrouched && (isMatched || armed->crouch != nullptr);

        if (!isMatched)
        {
            const LocomotionSet& set = isCrouchClip ? armedCrouchSet(*armed->crouch, view.isAlert)
                                                    : (view.isAlert ? *armed->aiming : *armed->rifle);
            updateLocomotion(look.locomotion, set, look.instance.animator(), foot, yaw, seconds, tuning,
                armed->body->rootJoint);
        }

        updateArmedCrouchLayer(look.instance.animator(), isMatched ? nullptr : armed->crouch,
            isCrouchClip && !isMatched, view.isAlert, k_crouchLayerFade);
        updateHitReaction(look.reaction, reactionRigFor(looks, *model), looks.reactionTuning, seconds);
        const float squash = isCrouched && !isCrouchClip ? k_crouchSquash : 1.0f;
        look.modelTransform = math::Transform{.position = foot,
            .rotation = facing,
            .scale = math::Vec3{1.0f, squash, 1.0f}};
        poseCharacter(look.instance, *armed->body, look.motion, tuning,
            CharacterPoseFrame{.placement = look.modelTransform,
                .offsets = look.reaction.offsets,
                .signals = &look.locomotion.signals,
                .yaw = yaw,
                .physics = &physics,
                .deltaSeconds = seconds});
        look.instance.submit(renderer, *model, look.modelTransform, render::k_defaultRenderLayer, parts,
            look.stains.surface);
    }

    void endNpcLooks(NpcLooks& looks, lockstep::physics::PhysicsWorld& physics, render::Renderer& renderer, float alpha,
        float seconds)
    {
        for (NpcLook& look : looks.looks)
        {
            if (!look.isDrawn)
            {
                dropRagdoll(physics, look);
            }
        }

        std::erase_if(looks.looks, [](const NpcLook& look) { return !look.isDrawn; });
        presentDroppedWeapons(looks.weapons, physics, renderer, alpha, seconds);
    }

    const HeldWeapon* npcHeldWeapon(NpcLooks& looks, const anim::SkinnedModel& model)
    {
        auto entry = std::ranges::find(looks.heldWeapons, &model, &NpcModelWeapon::model);

        if (entry == looks.heldWeapons.end())
        {
            entry = std::ranges::find(looks.heldWeapons, nullptr, &NpcModelWeapon::model);
            entry = entry != looks.heldWeapons.end() ? entry : looks.heldWeapons.begin();
            *entry = NpcModelWeapon{.model = &model, .weapon = findHeldWeapon(model)};
            std::clog << "Game: NPC weapon " << (entry->weapon ? "found" : "not found (it stays in the hand)")
                      << " on model '" << model.name << "'\n";
        }

        return entry->weapon ? &*entry->weapon : nullptr;
    }

    void dropNpcWeapon(lockstep::physics::PhysicsWorld& physics, NpcLooks& looks, NpcLook& look,
        const math::Vec3& linear, const math::Vec3& angular)
    {
        const HeldWeapon* weapon = look.model != nullptr ? npcHeldWeapon(looks, *look.model) : nullptr;

        if (look.isWeaponDropped || weapon == nullptr)
        {
            return;
        }

        look.isWeaponDropped = true;

        if (auto dropped = dropHeldWeapon(looks.weapons, physics, WeaponDrop{.model = *look.model,
                .instance = look.instance, .transform = drawnPlacement(look), .weapon = *weapon, .linear = linear,
                .angular = angular});
            !dropped)
        {
            std::clog << "Game: NPC weapon not dropped: " << dropped.error().message << '\n';
        }
    }

    void pushNpcBody(lockstep::physics::PhysicsWorld& physics, NpcLooks& looks, NpcLook& look, uint32_t body,
        const math::Vec3& impulse, const std::optional<math::Vec3>& point)
    {
        if (!look.ragdoll || look.model == nullptr)
        {
            return;
        }

        if (!isSimulating(*look.ragdoll) && !makeRoomForRagdoll(physics, looks, &*look.ragdoll))
        {
            return;
        }

        pushNpcRagdoll(physics, looks.ragdolls, *look.model, look.instance, *look.ragdoll, body, impulse, point);
    }

    void reactToNpcHit(NpcLooks& looks, NpcLook& look, const ReactionHit& hit)
    {
        const bool isPosed = look.model != nullptr && look.instance.jointMatrices().size() == look.model->skeleton.joints.size();

        if (!isPosed || look.ragdoll || look.deathSeconds > 0.0f || !looks.reactionTuning.isEnabled)
        {
            return;
        }

        const ReactionRig& rig = reactionRigFor(looks, *look.model);
        static_cast<void>(pushHitReaction(look.reaction, rig, look.instance.jointMatrices(), look.modelTransform, hit,
            looks.reactionTuning));
    }

    anim::PartMask npcBodyParts(NpcLooks& looks, const NpcLook& look)
    {
        if (look.model == nullptr)
        {
            return anim::k_allParts;
        }

        const HeldWeapon* weapon = look.isWeaponDropped ? npcHeldWeapon(looks, *look.model) : nullptr;
        const anim::PartMask parts = severedBodyParts(*look.model, look.gearParts, look.severedLimbs);
        return weapon != nullptr ? parts & ~weapon->parts : parts;
    }

    const math::Transform& drawnPlacement(const NpcLook& look)
    {
        return look.ragdoll ? look.ragdoll->placement : look.modelTransform;
    }

#if LOCKSTEP_DEV_TOOLS
    void npcLooksDevSection(NpcLooks& looks, const lockstep::physics::PhysicsWorld& physics)
    {
        uint32_t simulating = 0;
        uint32_t asleep = 0;
        uint32_t frozen = 0;

        for (const NpcLook& look : looks.looks)
        {
            if (look.ragdoll)
            {
                ++(isSimulating(*look.ragdoll) ? simulating : frozen);
                asleep += isSimulating(*look.ragdoll) && look.ragdoll->isAsleep ? 1u : 0u;
            }
        }

        npcRagdollsDevSection(looks.ragdolls, simulating, asleep, frozen, physics);
        ImGui::Checkbox("Draw every NPC crouched", &looks.isCrouchForced);

        if (ImGui::TreeNode("Dropped weapons"))
        {
            WeaponDropTuning& tuning = looks.weapons.tuning;
            int maxWeapons = static_cast<int>(tuning.maxWeapons);
            ImGui::Text("Lying around: %zu", looks.weapons.weapons.size());

            if (ImGui::SliderInt("Max weapons", &maxWeapons, 0, 64))
            {
                tuning.maxWeapons = static_cast<uint32_t>(maxWeapons);
            }

            ImGui::SliderFloat("Lifetime (s)", &tuning.lifeSeconds, 10.0f, 900.0f);
            ImGui::SliderFloat("Throw speed (m/s)", &tuning.speed, 0.0f, 6.0f);
            ImGui::SliderFloat("Throw lift (m/s)", &tuning.lift, 0.0f, 4.0f);
            ImGui::SliderFloat("Spin (rad/s)", &tuning.spin, 0.0f, 20.0f);
            ImGui::TreePop();
        }
    }
#endif
} //namespace game
