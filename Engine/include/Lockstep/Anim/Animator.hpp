#ifndef LOCKSTEP_ANIM_ANIMATOR_HPP
#define LOCKSTEP_ANIM_ANIMATOR_HPP

#include "Lockstep/Anim/AnimationClip.hpp"
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
    // loop:  wraps around at the end; otherwise holds the last frame.
    // speed: playback rate, 1 for authored speed.
    struct PlaybackOptions
    {
        bool loop{true};
        float speed{1.0f};
    };

    // Plays clips on one skeleton: a single clip or a 1D blend between clips by a parameter (such
    // as idle/walk/run by speed), and crossfades from the previous motion when a new one starts.
    // Clips of a blend share one normalized phase, so walk and run cycles stay in step.
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

        // Returns: true while a crossfade is in progress.
        [[nodiscard]] bool isFading() const noexcept;

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
        void start(Motion&& motion, float fadeSeconds);

        Motion m_current{};
        Motion m_previous{};
        bool m_hasCurrent{false};
        bool m_hasPrevious{false};
        float m_fadeSeconds{0.0f};
        float m_fadeElapsed{0.0f};
        float m_parameter{0.0f};
        Pose m_scratch{};
        Pose m_previousPose{};
    };
} //namespace lockstep::anim

#endif //LOCKSTEP_ANIM_ANIMATOR_HPP
