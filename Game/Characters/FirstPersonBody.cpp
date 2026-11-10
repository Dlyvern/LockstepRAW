#include "Characters/FirstPersonBody.hpp"

#include <Lockstep/Anim/Limbs.hpp>

#if LOCKSTEP_DEV_TOOLS
#include <imgui.h>
#endif

#include <algorithm>
#include <cmath>

namespace game
{
    namespace
    {
        namespace anim = lockstep::anim;

        constexpr float k_fullLookDown{1.2f};
    } //namespace

    std::optional<FirstPersonBodyMasks> findFirstPersonBodyMasks(const anim::SkinnedModel& model,
        const GearMasks& gear)
    {
        if (!anim::hasLimbs(model))
        {
            return std::nullopt;
        }

        const anim::PartMask headGear = gearMask(gear, GearLook::Full) & ~gearMask(gear, GearLook::Balaclava);
        const anim::PartMask head = anim::limbParts(model, anim::limbSubtree(anim::Limb::Head), anim::LimbPartRole::Region);
        const anim::PartMask arms = anim::limbParts(model,
            anim::limbSubtree(anim::Limb::UpperArmLeft) | anim::limbSubtree(anim::Limb::UpperArmRight),
            anim::LimbPartRole::Region);
        const anim::PartMask legs = anim::limbParts(model,
            anim::limbSubtree(anim::Limb::ThighLeft) | anim::limbSubtree(anim::Limb::ThighRight),
            anim::LimbPartRole::Region);
        const anim::PartMask body = anim::k_allParts & ~anim::woundCapParts(model) & ~headGear & ~head;

        return FirstPersonBodyMasks{.withArms = body, .withoutArms = body & ~arms, .legs = legs};
    }

    anim::PartMask firstPersonBodyParts(const FirstPersonBodyMasks& masks, const FirstPersonBodyTuning& tuning,
        const FirstPersonBodyView& view)
    {
        switch (tuning.mode)
        {
            case FirstPersonBodyMode::TorsoAndLegs:
                return view.hasViewModel ? masks.withoutArms : masks.withArms;
            case FirstPersonBodyMode::LegsOnly:
                return masks.legs;
            case FirstPersonBodyMode::Hidden:
                break;
        }

        return 0;
    }

    math::Transform firstPersonBodyPlacement(const math::Transform& placement, const FirstPersonBodyTuning& tuning,
        const FirstPersonBodyView& view, float crouch, float kick)
    {
        const float lookDown = std::clamp(-view.lookPitch / k_fullLookDown, 0.0f, 1.0f);
        const float back = tuning.backOffset + tuning.lookDownBack * lookDown
            + tuning.crouchBack * std::clamp(crouch, 0.0f, 1.0f) - tuning.kickForward * std::clamp(kick, 0.0f, 1.0f);
        const math::Vec3 forward{-std::sin(view.lookYaw), 0.0f, -std::cos(view.lookYaw)};
        math::Transform moved = placement;
        moved.position = placement.position - forward * back;

        return moved;
    }

#if LOCKSTEP_DEV_TOOLS
    void firstPersonBodyDevSection(FirstPersonBodyTuning& tuning)
    {
        if (!ImGui::TreeNode("First-person body"))
        {
            return;
        }

        const char* modes[] = {"Torso and legs", "Legs only", "Hidden"};
        int mode = static_cast<int>(tuning.mode);

        if (ImGui::Combo("Shown", &mode, modes, 3))
        {
            tuning.mode = static_cast<FirstPersonBodyMode>(mode);
        }

        ImGui::SliderFloat("Behind the eye (m)", &tuning.backOffset, -0.2f, 0.5f);
        ImGui::SliderFloat("Extra back looking down (m)", &tuning.lookDownBack, 0.0f, 0.5f);
        ImGui::SliderFloat("Extra back crouched (m)", &tuning.crouchBack, 0.0f, 0.5f);
        ImGui::SliderFloat("Forward with a kick (m)", &tuning.kickForward, 0.0f, 0.6f);
        ImGui::TextDisabled("Casts a shadow without head (the Engine has no per-draw shadow switch yet)");
        ImGui::TreePop();
    }
#endif
} //namespace game
