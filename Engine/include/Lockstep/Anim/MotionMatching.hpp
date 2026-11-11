#ifndef LOCKSTEP_ANIM_MOTION_MATCHING_HPP
#define LOCKSTEP_ANIM_MOTION_MATCHING_HPP

#include "Lockstep/Anim/MotionDatabase.hpp"
#include "Lockstep/Core/Math.hpp"

#include <cstdint>

namespace lockstep::anim
{
    // Plays a MotionDatabase: every searchInterval (or when forced, or at the end of a one-shot
    // clip) it searches the frame whose features best continue the current pose along the wanted
    // trajectory, and jumps there when that is clearly better than carrying on. The caller plays
    // the result with Animator::playFrom() (inertialization smooths the jump) and fits the in-place
    // motion to the character's real movement with the locomotion layers (LocomotionPose.hpp).
    // Plain data; presentation only.

    // Marks "no clip" in MotionMatchState::clip.
    inline constexpr uint32_t k_noMotionClip{0xFFFFFFFFu};

    // How a matcher searches.
    // searchInterval: seconds between searches.
    // minImprovement: a jump needs a cost this much lower than carrying on (normalised units).
    // sameClipWindow: a best frame in the playing clip closer than this (seconds) to the playing
    //                 time is no jump.
    struct MotionMatchSettings
    {
        float searchInterval{0.1f};
        float minImprovement{0.25f};
        float sameClipWindow{0.25f};
    };

    // One character's matcher state.
    // clip:         the playing clip slot (index into MotionDatabase::clips()), or k_noMotionClip.
    // seconds:      time in it.
    // sinceSearch:  seconds since the last search.
    // cost:         the cost of the frame playing at the last search.
    // jumps:        jumps made so far (dev panel).
    struct MotionMatchState
    {
        uint32_t clip{k_noMotionClip};
        float seconds{0.0f};
        float sinceSearch{0.0f};
        float cost{0.0f};
        uint32_t jumps{0};
    };

    // What to play after an update.
    // isJump:       a new clip or time was chosen: play `playClip` from `startPhase`.
    // playClip:     the model clip to play (the in-place copy).
    // startPhase:   where in it to start, 0 to 1.
    // isLooping:    it cycles.
    // rootVelocity: the playing frame's root velocity in character space, m/s (for stride and
    //               orientation warping).
    struct MotionMatchResult
    {
        bool isJump{false};
        uint32_t playClip{0};
        float startPhase{0.0f};
        bool isLooping{true};
        math::Vec3 rootVelocity{0.0f, 0.0f, 0.0f};
    };

    // Advances the playing time and searches when due.
    // database:     the database.
    // state:        the character's matcher state (reset it to start over, e.g. on a set change).
    // wanted:       the wanted trajectory, character space.
    // settings:     how to search.
    // deltaSeconds: presentation time since the last update.
    // forceSearch:  search now (a sudden change of the wanted movement).
    // Returns:      what to play.
    [[nodiscard]] MotionMatchResult updateMotionMatching(const MotionDatabase& database, MotionMatchState& state,
        const MotionTrajectory& wanted, const MotionMatchSettings& settings, float deltaSeconds, bool forceSearch);

    // Predicts a character's trajectory: its velocity and facing move towards wanted ones like
    // critically damped springs.
    // velocity:       current ground velocity, character space, m/s.
    // wantedVelocity: the velocity it is heading for, character space, m/s.
    // wantedYaw:      the facing it is heading for, relative to now (radians, positive towards +X).
    // halflife:       spring half-life, seconds.
    // Returns:        the trajectory at the database's trajectory times.
    [[nodiscard]] MotionTrajectory predictTrajectory(const math::Vec3& velocity, const math::Vec3& wantedVelocity,
        float wantedYaw, float halflife);
} //namespace lockstep::anim

#endif //LOCKSTEP_ANIM_MOTION_MATCHING_HPP
