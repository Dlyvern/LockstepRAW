#include "Npcs/NpcPresentation.hpp"

#include "Characters/PawnCharacters.hpp"
#include "Presentation/Visuals.hpp"

#include <Lockstep/Render/Lighting.hpp>
#include <Lockstep/Render/PbrMaterial.hpp>
#include <Lockstep/Render/Renderer.hpp>

#include <algorithm>
#include <cmath>
#include <iostream>

namespace game
{
    namespace
    {
        namespace anim = lockstep::anim;
        namespace render = lockstep::render;

        constexpr math::Vec3 k_upAxis{0.0f, 1.0f, 0.0f};
        constexpr math::Vec3 k_modelRight{1.0f, 0.0f, 0.0f};
        constexpr math::Vec3 k_forward{0.0f, 0.0f, -1.0f};
        constexpr math::Vec4 k_mercColour{0.055f, 0.058f, 0.065f, 1.0f};
        constexpr float k_mercRoughness{0.8f};
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

        const anim::SkinnedModel* mercModel(render::Renderer& renderer, NpcLooks& looks,
            const anim::SkinnedModel& armed)
        {
            if (!looks.isMercModelTried)
            {
                looks.isMercModelTried = true;
                auto material = renderer.createMaterial(render::PbrMaterialDescription{.baseColorFactor = k_mercColour,
                        .metallicFactor = 0.0f,
                        .roughnessFactor = k_mercRoughness},
                    render::MeshKind::Skinned);

                if (!material)
                {
                    std::clog << "Game: merc material not created (" << material.error().message
                              << "); mercs look like the squad\n";
                    return nullptr;
                }

                anim::SkinnedModel model = armed;

                for (anim::SkinnedModelPart& part : model.parts)
                {
                    part.material = material.value();
                }

                looks.mercModel = std::move(model);
            }

            return looks.mercModel ? &*looks.mercModel : nullptr;
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
    } //namespace

    void beginNpcLooks(NpcLooks& looks)
    {
        for (NpcLook& look : looks.looks)
        {
            look.isDrawn = false;
        }
    }

    void drawNpc(render::Renderer& renderer, NpcLooks& looks, const PawnCharacters* characters,
        const Visuals& visuals, Archetype archetype, lockstep::world::EntityId entity, const math::Transform& transform,
        const NpcView& view, float seconds)
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
        const anim::SkinnedModel* model = isMerc ? mercModel(renderer, looks, armed.body.skinned()) : nullptr;
        model = model != nullptr ? model : &armed.body.skinned();
        const anim::PartMask parts = gearMask(armed.body.gearMasks, view.gearLook);
        const math::Vec3 foot = transform.position - k_upAxis * (0.5f * transform.scale.y);
        const float yaw = yawOf(transform.rotation);
        const math::Quat facing = math::angleAxis(yaw + math::k_pi, k_upAxis);

        if (view.isDead)
        {
            const float before = look.deathSeconds;
            look.deathSeconds += seconds;
            const float fall = std::min(look.deathSeconds / k_fallSeconds, 1.0f);

            if (before < k_fallSeconds)
            {
                look.instance.update(*model, seconds);
            }

            look.instance.submit(renderer, *model, math::Transform{.position = foot,
                .rotation = facing * math::angleAxis(-0.5f * math::k_pi * fall * fall, k_modelRight)},
                render::k_defaultRenderLayer, parts);
            return;
        }

        look.deathSeconds = 0.0f;
        const LocomotionSet& set = view.isAlert ? armed.aiming : armed.rifle;
        updateLocomotion(look.locomotion, set, look.instance.animator(), foot, yaw, seconds);
        look.instance.update(*model, seconds);
        const float squash = transform.scale.y < k_crouchedHeight ? k_crouchSquash : 1.0f;
        look.instance.submit(renderer, *model, math::Transform{.position = foot,
            .rotation = facing,
            .scale = math::Vec3{1.0f, squash, 1.0f}},
            render::k_defaultRenderLayer, parts);
    }

    void endNpcLooks(NpcLooks& looks)
    {
        std::erase_if(looks.looks, [](const NpcLook& look) { return !look.isDrawn; });
    }
} //namespace game
