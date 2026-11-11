#ifndef LOCKSTEP_GAME_CHARACTERS_LOCOMOTION_SIGNALS_HPP
#define LOCKSTEP_GAME_CHARACTERS_LOCOMOTION_SIGNALS_HPP

#include <Lockstep/Anim/LocomotionPose.hpp>
#include <Lockstep/Anim/MotionMatching.hpp>
#include <Lockstep/Anim/SkinnedModel.hpp>
#include <Lockstep/Core/Math.hpp>
#include <Lockstep/Core/Transform.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace game
{
    namespace math = lockstep::math;

    // How the procedural locomotion layers on top of the clips behave, shared by pawns, NPCs and
    // placeholders. Each layer has a toggle for A/B comparison in the dev panel.
    // isInertialized:     clip changes blend inertially instead of crossfading.
    // halflifePerFade:    inertialization half-life per second of the old crossfade length.
    // isStrideWarped:     speed mismatches are split between play rate and stride length.
    // playbackShare:      0 to 1: how much of the mismatch the play rate takes (as a power of
    //                     the speed ratio); the rest lengthens or shortens the steps.
    // isOrientationWarped: legs turn towards the movement between the four clip directions, the
    //                     spine turns the upper body back to the facing.
    // warpSmoothing:      seconds the warp angle eases over.
    // isLeaning:          the body leans along its acceleration (into turns, back when braking).
    // leanPerAcceleration: radians per m/s^2 of acceleration.
    // leanFrequency, leanDamping: the lean spring (Hz, damping ratio; below 1 overshoots).
    // isTurningInPlace:   a standing character keeps its feet planted while its facing turns,
    //                     then steps round (turn clip, else a procedural shuffle); off plays the
    //                     turn clips by turn rate as before.
    // turnStartAngle:     how far the facing may turn away from the legs before a step, radians.
    // turnSettleAngle, turnSettleSeconds: a smaller lag is stepped out after the facing has been
    //                     still that long.
    // turnClipShare:      a turn clip is used when the lag is at least this share of the clip's
    //                     own turn; smaller lags shuffle.
    // turnClipRate:       play rate of the turn clips.
    // shuffleSeconds:     length of a procedural turn step.
    // stepHeight:         how high a shuffling foot lifts, metres.
    // pose:               foot IK and the limits of the corrections (Engine settings).
    // isMotionMatched:    armed characters whose model has motion sets pick their frames by
    //                     motion matching; off (or without sets) plays the clip blends.
    // matching:           how the matcher searches (Engine settings).
    // anticipation:       seconds of the current acceleration added to the wanted velocity.
    // turnAnticipation:   seconds of the current turn rate added to the wanted facing.
    // trajectoryHalflife: half-life of the predicted trajectory's springs, seconds.
    // matchFade:          fade (inertialization) length of a jump, seconds.
    // maxMatchWarp:       the most the legs are warped towards the movement in motion matching.
    struct LocomotionTuning
    {
        bool isInertialized{true};
        float halflifePerFade{0.3f};
        bool isStrideWarped{true};
        float playbackShare{0.55f};
        bool isOrientationWarped{true};
        float warpSmoothing{0.1f};
        bool isLeaning{true};
        float leanPerAcceleration{0.035f};
        float leanFrequency{1.6f};
        float leanDamping{0.55f};
        bool isTurningInPlace{true};
        float turnStartAngle{1.05f};
        float turnSettleAngle{0.3f};
        float turnSettleSeconds{0.6f};
        float turnClipShare{0.6f};
        float turnClipRate{1.15f};
        float shuffleSeconds{0.5f};
        float stepHeight{0.07f};
        lockstep::anim::LocomotionPoseSettings pose{};
        bool isMotionMatched{true};
        lockstep::anim::MotionMatchSettings matching{};
        float anticipation{0.3f};
        float turnAnticipation{0.35f};
        float trajectoryHalflife{0.25f};
        float matchFade{0.25f};
        float maxMatchWarp{0.8f};
    };

    // Samples of a turn clip's progress curve.
    inline constexpr std::size_t k_turnCurveSamples{9};

    // A turn clip and how far it turns the hips while it turns.
    // clip:       index into the model's clips.
    // angle:      radians, positive to the left.
    // startPhase: where in the clip (0 to 1) the turning starts; a step starts the clip there.
    // duration:   seconds the turning takes at play rate 1 (it ends there; the rest is idle).
    // progress:   share of `angle` turned at evenly spaced times over `duration` (0 to 1), so a
    //             turn step follows the clip's own timing.
    struct TurnClip
    {
        uint32_t clip{0};
        float angle{0.0f};
        float startPhase{0.0f};
        float duration{0.0f};
        std::array<float, k_turnCurveSamples> progress{};
    };

    // One step of turning in place.
    // seconds:   time into the step.
    // duration:  its length.
    // startLag:  the legs' yaw relative to the facing when it started (radians, positive left).
    // isLeft:    it turns to the left.
    // clip:      the turn clip it plays, or nothing for a procedural shuffle.
    struct TurnStep
    {
        float seconds{0.0f};
        float duration{0.0f};
        float startLag{0.0f};
        bool isLeft{false};
        std::optional<TurnClip> clip{};
    };

    // The procedural parts of one character's locomotion, derived from its observed movement.
    // Presentation only, never replicated.
    // bodyYaw:       world yaw the legs face (radians, 0 faces -Z, positive left).
    // hasBodyYaw:    bodyYaw is valid.
    // warpYaw:       smoothed orientation warp, radians (positive turns the legs left).
    // strideScale:   stride length relative to the playing clip's.
    // moveDirection: movement direction in model space (+Z forward, +X left), zero standing.
    // acceleration:  smoothed planar acceleration, world, m/s^2.
    // lastVelocity:  the velocity the acceleration was measured from.
    // lean, leanSpeed: lean spring per model axis (x left, z forward), radians and rad/s.
    // step:          the turn step in progress.
    // settleSeconds: how long a small lag has been held.
    // footLift, footLock: per leg, for the Engine pose input (see LocomotionPoseInput).
    struct LocomotionSignals
    {
        float bodyYaw{0.0f};
        bool hasBodyYaw{false};
        float warpYaw{0.0f};
        float strideScale{1.0f};
        math::Vec3 moveDirection{0.0f, 0.0f, 0.0f};
        math::Vec3 acceleration{0.0f, 0.0f, 0.0f};
        math::Vec3 lastVelocity{0.0f, 0.0f, 0.0f};
        math::Vec3 lean{0.0f, 0.0f, 0.0f};
        math::Vec3 leanSpeed{0.0f, 0.0f, 0.0f};
        std::optional<TurnStep> step{};
        float settleSeconds{0.0f};
        std::array<float, lockstep::anim::k_legCount> footLift{};
        std::array<float, lockstep::anim::k_legCount> footLock{};
    };

    // The turn clips of a locomotion set, as measured.
    // left, right: the clips, or nothing.
    struct TurnClips
    {
        std::optional<TurnClip> left{};
        std::optional<TurnClip> right{};
    };

    // Measures a turn clip: how far it turns the hips, where the turning starts and ends and its
    // progress curve.
    // model:   the character model.
    // hips:    its hips joint (where the turn is measured).
    // clip:    the clip, or nothing.
    // Returns: the measured clip, or nothing without a clip.
    [[nodiscard]] std::optional<TurnClip> measureTurnClip(const lockstep::anim::SkinnedModel& model, uint32_t hips,
        std::optional<uint32_t> clip);

    // Keeps the legs planted while a standing character's facing turns and steps them round once
    // it has turned far enough (see LocomotionTuning). While moving, the legs follow the facing.
    // signals:      the character's signals.
    // tuning:       the tuning.
    // yaw:          its facing this frame.
    // isMoving:     it walks or runs.
    // isTurnFree:   the facing is (nearly) still, so a small lag may be settled.
    // clips:        its turn clips.
    // deltaSeconds: presentation time since the last frame.
    void updateTurnInPlace(LocomotionSignals& signals, const LocomotionTuning& tuning, float yaw, bool isMoving,
        bool isTurnFree, const TurnClips& clips, float deltaSeconds);

    // Updates the orientation warp, stride direction and lean from the movement.
    // signals:      the character's signals.
    // tuning:       the tuning.
    // velocity:     smoothed planar velocity, world.
    // yaw:          its facing.
    // warpTarget:   the warp the playing clip needs (radians, positive left), 0 when standing.
    // deltaSeconds: presentation time since the last frame.
    void updateLocomotionSignals(LocomotionSignals& signals, const LocomotionTuning& tuning, const math::Vec3& velocity,
        float yaw, float warpTarget, float deltaSeconds);

    // signals: the character's signals.
    // yaw:     its facing this frame.
    // Returns: how far the legs are turned from the facing, radians, positive left.
    [[nodiscard]] float lowerBodyYaw(const LocomotionSignals& signals, float yaw);

    // The Engine pose input of a character this frame.
    // signals:      its signals, or null for a character without locomotion (only foot IK).
    // yaw:          its facing (ignored without signals).
    // placement:    the world transform the model is drawn with.
    // isGrounded:   it stands on the ground (foot IK on).
    // deltaSeconds: presentation time since the last frame.
    // Returns:      the input.
    [[nodiscard]] lockstep::anim::LocomotionPoseInput locomotionPoseInput(const LocomotionSignals* signals, float yaw,
        const math::Transform& placement, bool isGrounded, float deltaSeconds);

    // Splits a speed mismatch between play rate and stride length.
    // tuning:      the tuning (stride warping on or off, its share).
    // speed:       ground speed, m/s.
    // strideSpeed: the clip's stride speed at play rate 1, m/s.
    // minRate, maxRate: play rate limits.
    // Returns:     the play rate, and in `strideScale` the stride length scale.
    [[nodiscard]] float stridePlayback(const LocomotionTuning& tuning, float speed, float strideSpeed, float minRate,
        float maxRate, float& strideScale);

    // Turns every procedural layer and motion matching off (crossfades, no foot IK, warping, lean or
    // turn steps): the
    // locomotion as it was before them, for A/B comparisons (`--dev-locomotion-off`).
    // tuning: the tuning, changed in place.
    void disableLocomotionLayers(LocomotionTuning& tuning);

#if LOCKSTEP_DEV_TOOLS
    // Dev panel section: the layer toggles and their tuning.
    // tuning: the tuning, edited in place.
    void locomotionTuningDevSection(LocomotionTuning& tuning);
#endif
} //namespace game

#endif //LOCKSTEP_GAME_CHARACTERS_LOCOMOTION_SIGNALS_HPP
