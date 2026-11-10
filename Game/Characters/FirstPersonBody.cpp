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

        constexpr float k_minEdgeAngle{0.05f};
        constexpr float k_maxEdgeAngle{3.0f};
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
        const FirstPersonBodyView& view, const math::Vec3& hips, float kick)
    {
        const math::Vec3 forward{-std::sin(view.lookYaw), 0.0f, -std::cos(view.lookYaw)};
        const float below = std::max(view.eye.y - hips.y, 0.0f);
        const float hipsAhead = math::dot(hips - view.eye, forward);
        const float edge = std::clamp(tuning.viewHalfAngle - view.lookPitch, k_minEdgeAngle, k_maxEdgeAngle);
        const float visibleFrom = below * std::cos(edge) / std::sin(edge);
        const float needed = hipsAhead + tuning.hipRadius + tuning.margin - visibleFrom;
        const float preferred = tuning.minBack - tuning.kickForward * std::clamp(kick, 0.0f, 1.0f);
        math::Transform moved = placement;
        moved.position = placement.position - forward * std::max(needed, preferred);

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

        ImGui::SliderFloat("Least distance back (m)", &tuning.minBack, -0.3f, 0.5f);
        ImGui::SliderFloat("Thigh tops ahead of hips (m)", &tuning.hipRadius, 0.0f, 0.4f);
        ImGui::SliderFloat("Margin to view edge (m)", &tuning.margin, 0.0f, 0.4f);
        ImGui::SliderFloat("View half angle (rad)", &tuning.viewHalfAngle, 0.3f, 1.0f);
        ImGui::SliderFloat("Forward with a kick (m)", &tuning.kickForward, 0.0f, 0.6f);
        ImGui::TreePop();
    }
#endif
} //namespace game
