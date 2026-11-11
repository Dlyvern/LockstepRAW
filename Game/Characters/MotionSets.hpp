#ifndef LOCKSTEP_GAME_CHARACTERS_MOTION_SETS_HPP
#define LOCKSTEP_GAME_CHARACTERS_MOTION_SETS_HPP

#include <Lockstep/Anim/LocomotionPose.hpp>
#include <Lockstep/Anim/MotionDatabase.hpp>
#include <Lockstep/Anim/SkinnedModel.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace game
{
    // Which motion-matching set a character moves with.
    // Rifle:  rifle relaxed (low ready): walk/run, starts, stops, backwards, strafes, turns.
    // Aim:    rifle up: idle, walk and run in 8 directions, turns.
    // Crouch: crouched rifle: idles, walk in 8 directions, turns.
    // Pistol: pistol: idle, walk/run forward and back, strafes, arcs.
    enum class MotionStance : uint8_t
    {
        Rifle = 0,
        Aim,
        Crouch,
        Pistol
    };

    // Number of MotionStance values.
    inline constexpr std::size_t k_motionStanceCount{4};

    // The motion-matching databases of one model, per stance (nothing where the model has no
    // clips of that set). Built from the model's "MM_<Set>_*" clips (Content/source/scripts/
    // mm_retarget.py); clips whose name has Start, Stop or Turn play once, the rest loop.
    struct MotionSets
    {
        std::array<std::optional<lockstep::anim::MotionDatabase>, k_motionStanceCount> stances{};
    };

    // Builds a model's motion sets and appends their in-place clips to the model.
    // model:   the model (its clips are extended).
    // rig:     its locomotion rig.
    // Returns: the sets, or nothing when the model has no MM_ clips (failures are logged).
    [[nodiscard]] std::optional<MotionSets> buildMotionSets(lockstep::anim::SkinnedModel& model,
        const lockstep::anim::LocomotionRig& rig);

    // sets:    a model's sets.
    // stance:  the stance.
    // Returns: its database, or null when the model has none for it.
    [[nodiscard]] const lockstep::anim::MotionDatabase* motionDatabase(const MotionSets& sets, MotionStance stance);

    // Picks the stance from how a character is drawn.
    // isCrouched: crouched.
    // isAiming:   rifle up.
    // isPistol:   holds a pistol.
    // Returns:    the stance.
    [[nodiscard]] MotionStance motionStance(bool isCrouched, bool isAiming, bool isPistol);

    // Returns: a short name of a stance, for the dev panel.
    [[nodiscard]] const char* motionStanceName(MotionStance stance);
} //namespace game

#endif //LOCKSTEP_GAME_CHARACTERS_MOTION_SETS_HPP
