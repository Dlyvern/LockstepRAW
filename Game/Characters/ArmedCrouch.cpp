#include "Characters/ArmedCrouch.hpp"

#include <array>

namespace game
{
    namespace
    {
        namespace anim = lockstep::anim;

        std::optional<uint32_t> findBareJoint(const anim::Skeleton& skeleton, std::string_view name)
        {
            for (uint32_t joint = 0; joint < skeleton.joints.size(); ++joint)
            {
                const std::string_view jointName = skeleton.joints[joint].name;
                const std::size_t separator = jointName.find_last_of(':');
                const std::string_view bare = separator == std::string_view::npos ? jointName
                                                                                   : jointName.substr(separator + 1);

                if (bare == name)
                {
                    return joint;
                }
            }

            return std::nullopt;
        }

        std::optional<LocomotionSet> optionalSet(const anim::SkinnedModel& model, const LocomotionClipNames& names)
        {
            auto set = findLocomotionSet(model, names);
            return set ? std::optional<LocomotionSet>(set.value()) : std::nullopt;
        }
    } //namespace

    std::optional<ArmedCrouch> findArmedCrouch(const anim::SkinnedModel& model, const ArmedCrouchNames& names)
    {
        ArmedCrouch crouch{.rifle = optionalSet(model, names.rifle), .aiming = optionalSet(model, names.aiming)};
        const std::optional<LocomotionSet> legs = optionalSet(model, names.legs);
        const std::optional<uint32_t> rifleUpper = anim::findClip(model.clips, names.rifleUpper);
        const std::optional<uint32_t> aimUpper = anim::findClip(model.clips, names.aimUpper);
        const std::optional<uint32_t> upperRoot = findBareJoint(model.skeleton, names.upperRoot);
        const bool isLayered = legs && rifleUpper && aimUpper && upperRoot;

        if (crouch.rifle && crouch.aiming)
        {
            crouch.legs = *crouch.aiming;
            return crouch;
        }

        if (!isLayered)
        {
            return std::nullopt;
        }

        const std::array<uint32_t, 1> roots{*upperRoot};
        crouch.legs = *legs;
        crouch.rifleUpper = *rifleUpper;
        crouch.aimUpper = *aimUpper;
        crouch.upperMask = anim::makeBoneMask(model.skeleton, roots, true);

        return crouch;
    }

    const LocomotionSet& armedCrouchSet(const ArmedCrouch& crouch, bool isAiming)
    {
        const std::optional<LocomotionSet>& real = isAiming ? crouch.aiming : crouch.rifle;

        return real ? *real : crouch.legs;
    }

    void updateArmedCrouchLayer(anim::Animator& animator, const ArmedCrouch* crouch, bool isCrouched, bool isAiming,
        float fadeSeconds)
    {
        const bool isLayered = crouch != nullptr && !(isAiming ? crouch->aiming : crouch->rifle);

        if (!isCrouched || !isLayered)
        {
            if (animator.layerWeight() > 0.0f)
            {
                animator.stopLayer(fadeSeconds);
            }

            return;
        }

        animator.playLayer(isAiming ? crouch->aimUpper : crouch->rifleUpper, crouch->upperMask, fadeSeconds);
    }
} //namespace game
