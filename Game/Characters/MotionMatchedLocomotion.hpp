#ifndef LOCKSTEP_GAME_CHARACTERS_MOTION_MATCHED_LOCOMOTION_HPP
#define LOCKSTEP_GAME_CHARACTERS_MOTION_MATCHED_LOCOMOTION_HPP

#include "Characters/Locomotion.hpp"
#include "Characters/MotionSets.hpp"

#include <Lockstep/Anim/Animator.hpp>

namespace game
{
    // Plays a character by motion matching instead of clip blends: observes where it is drawn (as
    // updateLocomotion() does), predicts the trajectory it is heading for from its smoothed
    // velocity, acceleration and turn rate (springs, character space), searches the stance's
    // database every LocomotionTuning::matching.searchInterval (and at once on a stance change or a
    // sudden change of the wanted velocity), plays jumps inertialized, and sets the signals the
    // locomotion layers then apply: orientation warp from the playing frame's root direction to the
    // real movement, stride scale from its root speed, and lean. Turning on the spot comes from the
    // matched turn clips, so the turn-in-place layer stays off. Presentation only.
    // state:        the character's tracking.
    // sets:         its model's motion sets.
    // stance:       the set to move with.
    // animator:     the character's animator.
    // foot:         where its feet are this frame.
    // yaw:          which way it faces this frame.
    // deltaSeconds: presentation time since the previous frame.
    // tuning:       the locomotion tuning.
    // Returns:      false when the sets have no database for `stance` (nothing was done; play the
    //               clip blends instead).
    bool updateMotionMatchedLocomotion(LocomotionState& state, const MotionSets& sets, MotionStance stance,
        lockstep::anim::Animator& animator, const math::Vec3& foot, float yaw, float deltaSeconds,
        const LocomotionTuning& tuning);
} //namespace game

#endif //LOCKSTEP_GAME_CHARACTERS_MOTION_MATCHED_LOCOMOTION_HPP
