#include "Npcs/NpcPresentation.hpp"

#include "Characters/PawnCharacters.hpp"
#include "Gore/SeveredLimbs.hpp"
#include "Presentation/Visuals.hpp"

#include <Lockstep/Physics/PhysicsWorld.hpp>
#include <Lockstep/Render/Lighting.hpp>
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
        constexpr float k_flashSeconds{0.05f};
        constexpr float k_flashIntensity{60.0f};
        constexpr float k_flashRange{6.0f};
        constexpr math::Vec3 k_flashColour{1.0f, 0.72f, 0.38f};
        constexpr float k_muzzleReach{0.7f};
        constexpr float k_muzzleDrop{0.15f};

        float yawOf(const math::Quat& rotation)
        {
            const math::Vec3 forward = rotation * k_forward;
            return std::atan2(-forward.x, -forward.z);
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

        void flashMuzzle(render::Renderer& renderer, NpcLook& look, const math::Transform& transform,
            const NpcView& view, float seconds)
        {
            if (view.shotCount != look.shotCount)
            {
                look.shotCount = view.shotCount;
                look.flashSeconds = k_flashSeconds;
            }

            if (look.flashSeconds <= 0.0f)
            {
                return;
            }

            look.flashSeconds -= seconds;
            const math::Vec3 forward = transform.rotation * k_forward;
            const math::Vec3 eye = transform.position + k_upAxis * (0.5f * transform.scale.y - 0.2f);
            renderer.submitLight(render::PointLight{.position = eye + forward * k_muzzleReach - k_upAxis * k_muzzleDrop,
                .color = k_flashColour,
                .intensity = k_flashIntensity,
                .range = k_flashRange});
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
            look.instance.submit(renderer, model, look.ragdoll->placement, render::k_defaultRenderLayer, parts);
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
        flashMuzzle(renderer, look, transform, view, seconds);
        const bool isMerc = archetype == Archetype::Merc;

        if (characters == nullptr || !characters->armed)
        {
            renderer.submit(visuals.cube, isMerc ? visuals.enemy : visuals.player1, transform);
            return;
        }

        const ArmedCharacter& armed = *characters->armed;
        const anim::SkinnedModel* model = &armed.body.skinned();
        look.model = model;
        look.gearParts = gearMask(armed.body.gearMasks, view.gearLook);
        const math::Vec3 foot = transform.position - k_upAxis * (0.5f * transform.scale.y);
        const float yaw = yawOf(transform.rotation);
        const math::Quat facing = math::angleAxis(yaw + math::k_pi, k_upAxis);

        if (view.isDead)
        {
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
            look.instance.submit(renderer, *model, look.modelTransform, render::k_defaultRenderLayer, parts);
            return;
        }

        dropRagdoll(physics, look);
        look.deathSeconds = 0.0f;
        look.isWeaponDropped = false;
        const anim::PartMask parts = npcBodyParts(looks, look);
        const LocomotionSet& set = view.isAlert ? armed.aiming : armed.rifle;
        updateLocomotion(look.locomotion, set, look.instance.animator(), foot, yaw, seconds);
        look.instance.update(*model, seconds);
        const float squash = transform.scale.y < k_crouchedHeight ? k_crouchSquash : 1.0f;
        look.modelTransform = math::Transform{.position = foot,
            .rotation = facing,
            .scale = math::Vec3{1.0f, squash, 1.0f}};
        look.instance.submit(renderer, *model, look.modelTransform, render::k_defaultRenderLayer, parts);
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
        if (looks.weaponModel != &model)
        {
            looks.weaponModel = &model;
            looks.heldWeapon = findHeldWeapon(model);
            std::clog << "Game: NPC weapon " << (looks.heldWeapon ? "found" : "not found (it stays in the hand)")
                      << " on model '" << model.name << "'\n";
        }

        return looks.heldWeapon ? &*looks.heldWeapon : nullptr;
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
