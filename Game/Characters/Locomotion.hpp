#ifndef LOCKSTEP_GAME_CHARACTERS_LOCOMOTION_HPP
#define LOCKSTEP_GAME_CHARACTERS_LOCOMOTION_HPP

#include "Characters/LocomotionSignals.hpp"

#include <Lockstep/Anim/Animator.hpp>
#include <Lockstep/Anim/MotionMatching.hpp>
#include <Lockstep/Anim/RootMotion.hpp>
#include <Lockstep/Anim/SkinnedModel.hpp>
#include <Lockstep/Core/Math.hpp>
#include <Lockstep/Core/Result.hpp>

#include <cstdint>
#include <optional>
#include <string_view>

namespace game
{
    namespace math = lockstep::math;

    // Names of the locomotion clips in a character model (compared case-insensitively). The turn
    // clips are optional; an empty name means the model has none. reverseWalkBack plays the
    // walkBack clip backwards (for sets that reuse a forward walk, e.g. crouching).
    struct LocomotionClipNames
    {
        std::string_view idle{"Idle"};
        std::string_view walk{"Walk"};
        std::string_view run{"Run"};
        std::string_view walkBack{"WalkBack"};
        std::string_view strafeLeft{"StrafeLeft"};
        std::string_view strafeRight{"StrafeRight"};
        std::string_view turnLeft{"TurnLeft"};
        std::string_view turnRight{"TurnRight"};
        bool reverseWalkBack{false};
    };

    // A moving clip and the ground speed its feet cover at authored playback speed, so playback
    // can be scaled until the feet stop sliding.
    // clip:        index into the model's clips.
    // strideSpeed: metres per second of the planted foot relative to the body.
    // direction:   1 plays the clip forwards, -1 backwards.
    struct StrideClip
    {
        uint32_t clip{0};
        float strideSpeed{1.0f};
        float direction{1.0f};
    };

    // The locomotion clips of one character model.
    // turnClips: the turn clips measured for turning in place (how far they turn the hips).
    struct LocomotionSet
    {
        uint32_t idle{0};
        StrideClip walk{};
        StrideClip run{};
        StrideClip walkBack{};
        StrideClip strafeLeft{};
        StrideClip strafeRight{};
        std::optional<uint32_t> turnLeft{};
        std::optional<uint32_t> turnRight{};
        TurnClips turnClips{};
    };

    // What a character is doing, picked from its observed movement.
    enum class Gait : uint8_t
    {
        Idle = 0,
        Forward,
        Backward,
        StrafeLeft,
        StrafeRight,
        TurnLeft,
        TurnRight
    };

    // Presentation-side movement tracking of one character, from where it is drawn each frame.
    // Never replicated.
    // lastFoot:  foot position drawn last frame.
    // lastYaw:   yaw drawn last frame (radians, 0 faces -Z, positive turns left).
    // velocity:  smoothed planar velocity in m/s.
    // yawRate:   smoothed turn rate in rad/s, positive to the left.
    // gait:      the motion being played.
    // hasLast:   lastFoot and lastYaw are valid (false after a gap or a teleport).
    // verticalSpeed: smoothed vertical speed of the feet, m/s (judges jumps of remote pawns).
    // signals:   the procedural layers derived from the movement (warps, lean, turn steps).
    // matcher:   motion matching state (see MotionMatchedLocomotion.hpp).
    // stance:    the motion set matched last, or -1 when the clip blends played.
    // wanted:    the wanted velocity the last trajectory was predicted towards, character space.
    struct LocomotionState
    {
        math::Vec3 lastFoot{0.0f, 0.0f, 0.0f};
        float lastYaw{0.0f};
        math::Vec3 velocity{0.0f, 0.0f, 0.0f};
        float yawRate{0.0f};
        Gait gait{Gait::Idle};
        bool hasLast{false};
        float verticalSpeed{0.0f};
        LocomotionSignals signals{};
        lockstep::anim::MotionMatchState matcher{};
        int32_t stance{-1};
        math::Vec3 wanted{0.0f, 0.0f, 0.0f};
    };

    // Finds a model's locomotion clips and measures each moving clip's stride speed from its
    // planted feet (joints named "...LeftFoot" / "...RightFoot"). Falls back to typical speeds
    // when the feet can't be found.
    // model:   the character model.
    // names:   the clip names.
    // Returns: the clips, or InvalidArgument naming the first required clip that is missing.
    [[nodiscard]] lockstep::Result<LocomotionSet> findLocomotionSet(const lockstep::anim::SkinnedModel& model,
        const LocomotionClipNames& names);

    // Observes where a character is drawn this frame and plays the matching motion: idle, walking
    // forward (only the walk clip, sped up a little, until the speed passes the walk stride by
    // 15%; then blending into the run clip, reaching it at the run stride), walking backward,
    // strafing, or turning on the spot, with playback scaled to the ground speed (split with the
    // stride length when stride warping is on) and crossfades or inertial blends between them.
    // Also updates the procedural signals (orientation warp, lean, turn steps; see
    // LocomotionSignals.hpp) the pose corrections are applied from.
    // state:        the character's tracking.
    // set:          the model's locomotion clips.
    // animator:     the character's animator.
    // foot:         where its feet are this frame.
    // yaw:          which way it faces this frame.
    // deltaSeconds: Player time since the previous frame.
    // tuning:       which procedural layers are on, and how they behave.
    // root:         the model's hips (turn clips are played with their turn taken out there).
    void updateLocomotion(LocomotionState& state, const LocomotionSet& set, lockstep::anim::Animator& animator,
        const math::Vec3& foot, float yaw, float deltaSeconds, const LocomotionTuning& tuning,
        const lockstep::anim::RootJoint& root);

    // Tracks where a character is drawn: its smoothed velocity, turn rate and vertical speed (with
    // teleports ignored). The first call after a gap only remembers the position.
    // state:        the character's tracking.
    // foot:         where its feet are this frame.
    // yaw:          which way it faces this frame.
    // deltaSeconds: presentation time since the previous frame.
    // Returns:      true when the velocities were updated (not the first call, time passed).
    bool observeLocomotion(LocomotionState& state, const math::Vec3& foot, float yaw, float deltaSeconds);

    // Returns: a short name of a gait, for the dev panel.
    [[nodiscard]] const char* gaitName(Gait gait);
} //namespace game

#endif //LOCKSTEP_GAME_CHARACTERS_LOCOMOTION_HPP
