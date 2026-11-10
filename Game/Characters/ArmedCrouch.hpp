#ifndef LOCKSTEP_GAME_CHARACTERS_ARMED_CROUCH_HPP
#define LOCKSTEP_GAME_CHARACTERS_ARMED_CROUCH_HPP

#include "Characters/Locomotion.hpp"

#include <Lockstep/Anim/Animator.hpp>
#include <Lockstep/Anim/BoneMask.hpp>
#include <Lockstep/Anim/SkinnedModel.hpp>

#include <cstdint>
#include <optional>
#include <string_view>

namespace game
{
    // Clip names of crouched armed locomotion.
    // rifle, aiming: real crouched rifle clips (relaxed, weapon up); used when the model has them.
    // legs:          the unarmed crouch clips the legs of the layered crouch come from.
    // rifleUpper, aimUpper: the standing clips whose upper body is layered over the crouched legs
    //                (relaxed rifle, weapon up).
    // upperRoot:     the joint (name ending) where the upper body starts; it keeps its standing
    //                orientation in model space, so the rifle stays on its aim and the head level.
    struct ArmedCrouchNames
    {
        LocomotionClipNames rifle{};
        LocomotionClipNames aiming{};
        LocomotionClipNames legs{};
        std::string_view rifleUpper;
        std::string_view aimUpper;
        std::string_view upperRoot;
    };

    // The armed crouch of every armed body (Mixamo SWAT skeleton). Replace the layered crouch by
    // adding clips with the `rifle` / `aiming` names to the models.
    inline constexpr ArmedCrouchNames k_armedCrouchNames{
        .rifle = {.idle = "RifleCrouchIdle", .walk = "RifleCrouchWalk", .run = "RifleCrouchWalk",
            .walkBack = "RifleCrouchWalk", .strafeLeft = "RifleCrouchWalk", .strafeRight = "RifleCrouchWalk",
            .turnLeft = "", .turnRight = "", .reverseWalkBack = true},
        .aiming = {.idle = "AimCrouchIdle", .walk = "AimCrouchWalk", .run = "AimCrouchWalk",
            .walkBack = "AimCrouchWalk", .strafeLeft = "AimCrouchWalk", .strafeRight = "AimCrouchWalk",
            .turnLeft = "", .turnRight = "", .reverseWalkBack = true},
        .legs = {.idle = "CrouchIdle", .walk = "CrouchWalk", .run = "CrouchWalk", .walkBack = "CrouchWalk",
            .strafeLeft = "CrouchWalk", .strafeRight = "CrouchWalk", .turnLeft = "", .turnRight = "",
            .reverseWalkBack = true},
        .rifleUpper = "RifleIdle",
        .aimUpper = "AimIdle",
        .upperRoot = "Spine"};

    // Crouched locomotion of one armed model: real clips when it has them, else a layered crouch
    // (legs from the crouch clips, upper body from a standing rifle clip on an override layer).
    // rifle, aiming: the real crouched sets, if the model has them.
    // legs:          the crouch clips the layered crouch walks with.
    // rifleUpper, aimUpper: clips layered over the upper body.
    // upperMask:     the upper body joints.
    struct ArmedCrouch
    {
        std::optional<LocomotionSet> rifle{};
        std::optional<LocomotionSet> aiming{};
        LocomotionSet legs{};
        uint32_t rifleUpper{0};
        uint32_t aimUpper{0};
        lockstep::anim::BoneMask upperMask{};
    };

    // Finds the armed crouch of a model.
    // model:   the armed model.
    // names:   the clip and joint names.
    // Returns: the crouch, or nothing when the model has neither real crouched rifle clips nor
    //          the crouch, upper body clips and upper body joint of the layered one.
    [[nodiscard]] std::optional<ArmedCrouch> findArmedCrouch(const lockstep::anim::SkinnedModel& model,
        const ArmedCrouchNames& names);

    // crouch:   a model's armed crouch.
    // isAiming: the weapon is up.
    // Returns:  the locomotion clips to walk the crouch with.
    [[nodiscard]] const LocomotionSet& armedCrouchSet(const ArmedCrouch& crouch, bool isAiming);

    // Turns the upper body layer on while a layered crouch plays and off otherwise. Call every
    // frame for every armed character, crouched or not.
    // animator:    the character's animator.
    // crouch:      its model's armed crouch, or null (the layer goes off).
    // isCrouched:  it is drawn crouched with `crouch`.
    // isAiming:    the weapon is up.
    // fadeSeconds: how long the layer takes to come in or go.
    void updateArmedCrouchLayer(lockstep::anim::Animator& animator, const ArmedCrouch* crouch, bool isCrouched,
        bool isAiming, float fadeSeconds);
} //namespace game

#endif //LOCKSTEP_GAME_CHARACTERS_ARMED_CROUCH_HPP
