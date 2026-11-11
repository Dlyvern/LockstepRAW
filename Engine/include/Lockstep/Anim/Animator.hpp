#ifndef LOCKSTEP_ANIM_ANIMATOR_HPP
#define LOCKSTEP_ANIM_ANIMATOR_HPP

#include "Lockstep/Anim/AnimationClip.hpp"
#include "Lockstep/Anim/BoneMask.hpp"
#include "Lockstep/Anim/Inertialization.hpp"
#include "Lockstep/Anim/RootMotion.hpp"
#include "Lockstep/Anim/Skeleton.hpp"

#include <cstdint>
#include <span>
#include <vector>

namespace lockstep::anim
{
    // One clip of a 1D blend and the parameter value where it plays alone.
    // clip:      index into the model's clips.
    // parameter: where on the blend axis the clip sits (for idle/walk/run by speed: 0, 1.5, 4.5).
    struct BlendSample
    {
        uint32_t clip{0};
        float parameter{0.0f};
    };

    // How a motion plays.
    // loop:          wraps around at the end; otherwise holds the last frame.
    // speed:         playback rate, 1 for authored speed.
    // removeRootYaw: the turning the clips give the root joint (Animator::setRootJoint()) is
    //                taken out, so the motion keeps facing its start (the caller turns the body).
    // startPhase:    where in its cycle (0 to 1) a motion starts when it is started anew.
    struct PlaybackOptions
    {
        bool loop{true};
        float speed{1.0f};
        bool removeRootYaw{false};
        float startPhase{0.0f};
    };

    // Plays clips on one skeleton: a single clip or a 1D blend between clips by a parameter (such
    // as idle/walk/run by speed), and crossfades (or inertially blends, see
    // setInertialization()) from the previous motion when a new one starts.
    // Clips of a blend share one normalized phase, so walk and run cycles stay in step. An
    // optional override layer plays one more clip on the joints of a BoneMask (e.g. a rifle-aiming
    // upper body over crouched legs), faded in and out by weight.
    // Plain data with no pointers into models; every call that needs the clips takes them, so an
    // Animator can live in Game state. Deterministic: time only moves by advance().
    class Animator
    {
    public:
        // Starts playing one clip, fading from the current motion. Asking for the motion that is
        // already playing only updates its options, so this may be called every frame.
        // clip:        index into the clips later passed to advance() and evaluate().
        // fadeSeconds: crossfade length; 0 switches at once.
        // options:     looping and speed.
        void play(uint32_t clip, float fadeSeconds = 0.2f, const PlaybackOptions& options = {});

        // Starts playing a 1D blend, fading from the current motion. Asking for the blend that is
        // already playing only updates its options.
        // samples:     the clips and their parameter values, any order; must not be empty.
        // fadeSeconds: crossfade length; 0 switches at once.
        // options:     looping and speed.
        void playBlend(std::span<const BlendSample> samples, float fadeSeconds = 0.2f,
            const PlaybackOptions& options = {});

        // Starts playing one clip from a given point, fading from the current motion, even when the
        // same clip is already playing (motion matching jumps within clips).
        // clip:        index into the clips later passed to advance() and evaluate().
        // startPhase:  where in the clip to start, 0 to 1.
        // fadeSeconds: crossfade length (or inertialization, see setInertialization()); 0 switches
        //              at once.
        // options:     looping and speed (startPhase in it is ignored).
        void playFrom(uint32_t clip, float startPhase, float fadeSeconds, const PlaybackOptions& options = {});

        // Sets the blend parameter. Values outside the samples' range play the nearest end clip.
        // value: the parameter, e.g. ground speed in m/s.
        void setParameter(float value);

        // Returns: the blend parameter.
        [[nodiscard]] float parameter() const noexcept;

        // Moves time forward.
        // clips:        the model's clips (for their durations).
        // deltaSeconds: elapsed presentation time; negative is treated as 0.
        void advance(std::span<const AnimationClip> clips, float deltaSeconds);

        // Computes the current pose.
        // skeleton: the skeleton the clips animate.
        // clips:    the model's clips.
        // pose:     receives the pose.
        void evaluate(const Skeleton& skeleton, std::span<const AnimationClip> clips, Pose& pose);

        // Starts (or keeps) the override layer: `clip` replaces the base motion on the joints of
        // `mask`, its weight easing to 1. Asking for the clip already playing only updates the mask
        // and options; another clip crossfades from the old one.
        // clip:        index into the clips later passed to advance() and evaluate().
        // mask:        the joints it takes over.
        // fadeSeconds: time to fade in (and to crossfade from another layer clip); 0 is at once.
        // options:     looping and speed of the layer clip (it keeps its own time).
        void playLayer(uint32_t clip, const BoneMask& mask, float fadeSeconds = 0.2f,
            const PlaybackOptions& options = {});

        // Fades the override layer out (its weight eases to 0); it stops once faded.
        // fadeSeconds: time to fade out; 0 is at once.
        void stopLayer(float fadeSeconds = 0.2f);

        // Returns: the override layer's current weight, 0 (off) to 1.
        [[nodiscard]] float layerWeight() const noexcept;

        // Returns: true while a crossfade or an inertial blend is in progress.
        [[nodiscard]] bool isFading() const noexcept;

        // Chooses how the base motion changes when play() or playBlend() starts a new one: a
        // crossfade (the default), or inertialization (see PoseInertializer): the new motion
        // plays at once and the difference to the pose shown decays with a half-life of
        // `halflifePerFadeSecond` times the fade length asked for. The override layer keeps
        // crossfading. Takes effect from the next transition.
        // halflifePerFadeSecond: half-life per second of fade; 0 or less crossfades.
        void setInertialization(float halflifePerFadeSecond);

        // Returns: the half-life per fade second set by setInertialization(), 0 when crossfading.
        [[nodiscard]] float inertialization() const noexcept;

        // Sets the root joint PlaybackOptions::removeRootYaw takes turning out of.
        // root: the root joint (see makeRootJoint()).
        void setRootJoint(const RootJoint& root);

        // Returns: the playing motion's position in its cycle, 0 to 1.
        [[nodiscard]] float phase() const noexcept;

    private:
        struct Motion
        {
            std::vector<BlendSample> samples;
            PlaybackOptions options{};
            float phase{0.0f};
        };

        struct BlendPair
        {
            uint32_t first{0};
            uint32_t second{0};
            float weight{0.0f};
        };

        BlendPair blendPair(const Motion& motion) const;
        float duration(const Motion& motion, std::span<const AnimationClip> clips) const;
        void advanceMotion(Motion& motion, std::span<const AnimationClip> clips, float deltaSeconds) const;
        void evaluateMotion(const Motion& motion, const Skeleton& skeleton, std::span<const AnimationClip> clips,
            Pose& pose);
        void sampleMotionClip(const Motion& motion, const AnimationClip& clip, Pose& pose) const;
        void start(Motion&& motion, float fadeSeconds);
        void beginInertialization(const Skeleton& skeleton, std::span<const AnimationClip> clips, const Pose& pose,
            float deltaSeconds);
        void advanceLayer(std::span<const AnimationClip> clips, float deltaSeconds);
        void applyLayer(const Skeleton& skeleton, std::span<const AnimationClip> clips, Pose& pose);

        Motion m_current{};
        Motion m_previous{};
        bool m_hasCurrent{false};
        bool m_hasPrevious{false};
        float m_fadeSeconds{0.0f};
        float m_fadeElapsed{0.0f};
        float m_parameter{0.0f};
        Pose m_scratch{};
        Pose m_previousPose{};
        PoseInertializer m_inertializer{};
        RootJoint m_root{};
        Pose m_inertialBefore{};
        float m_inertialScale{0.0f};
        float m_pendingHalflife{0.0f};
        float m_unevaluatedSeconds{0.0f};
        bool m_hasPendingInertialization{false};
        Motion m_layer{};
        Motion m_previousLayer{};
        BoneMask m_layerMask{};
        bool m_hasLayer{false};
        bool m_hasPreviousLayer{false};
        float m_layerWeight{0.0f};
        float m_layerTarget{0.0f};
        float m_layerFadeSeconds{0.0f};
        float m_layerBlendSeconds{0.0f};
        float m_layerBlendElapsed{0.0f};
        Pose m_layerPose{};
        Pose m_previousLayerPose{};
    };
} //namespace lockstep::anim

#endif //LOCKSTEP_ANIM_ANIMATOR_HPP
