#include "Lockstep/Anim/Animator.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

namespace lockstep::anim
{
    namespace
    {
        float clipDuration(std::span<const AnimationClip> clips, uint32_t clip)
        {
            return clip < clips.size() ? clips[clip].duration : 0.0f;
        }

        bool sameSamples(std::span<const BlendSample> left, std::span<const BlendSample> right)
        {
            return std::equal(left.begin(), left.end(), right.begin(), right.end(),
                [](const BlendSample& a, const BlendSample& b)
                {
                    return a.clip == b.clip && a.parameter == b.parameter;
                });
        }
    } //namespace

    void Animator::play(uint32_t clip, float fadeSeconds, const PlaybackOptions& options)
    {
        const BlendSample sample{.clip = clip, .parameter = 0.0f};
        playBlend(std::span<const BlendSample>(&sample, 1), fadeSeconds, options);
    }

    void Animator::playBlend(std::span<const BlendSample> samples, float fadeSeconds, const PlaybackOptions& options)
    {
        if (samples.empty())
        {
            return;
        }

        Motion motion{};
        motion.samples.assign(samples.begin(), samples.end());
        motion.options = options;
        std::stable_sort(motion.samples.begin(), motion.samples.end(),
            [](const BlendSample& a, const BlendSample& b) { return a.parameter < b.parameter; });

        if (m_hasCurrent && sameSamples(m_current.samples, motion.samples))
        {
            m_current.options = options;
            return;
        }

        start(std::move(motion), fadeSeconds);
    }

    void Animator::setParameter(float value)
    {
        m_parameter = value;
    }

    float Animator::parameter() const noexcept
    {
        return m_parameter;
    }

    void Animator::advance(std::span<const AnimationClip> clips, float deltaSeconds)
    {
        const float delta = std::max(deltaSeconds, 0.0f);

        if (m_hasCurrent)
        {
            advanceMotion(m_current, clips, delta);
        }

        if (!m_hasPrevious)
        {
            return;
        }

        advanceMotion(m_previous, clips, delta);
        m_fadeElapsed += delta;

        if (m_fadeElapsed >= m_fadeSeconds)
        {
            m_hasPrevious = false;
            m_previous = Motion{};
        }
    }

    void Animator::evaluate(const Skeleton& skeleton, std::span<const AnimationClip> clips, Pose& pose)
    {
        if (!m_hasCurrent)
        {
            resetToRest(skeleton, pose);
            return;
        }

        evaluateMotion(m_current, skeleton, clips, pose);

        if (!m_hasPrevious || m_fadeSeconds <= 0.0f)
        {
            return;
        }

        evaluateMotion(m_previous, skeleton, clips, m_previousPose);
        blendPoses(m_previousPose, pose, m_fadeElapsed / m_fadeSeconds, pose);
    }

    bool Animator::isFading() const noexcept
    {
        return m_hasPrevious;
    }

    float Animator::phase() const noexcept
    {
        return m_current.phase;
    }

    Animator::BlendPair Animator::blendPair(const Motion& motion) const
    {
        const std::vector<BlendSample>& samples = motion.samples;
        const auto last = static_cast<uint32_t>(samples.size() - 1);

        if (samples.size() == 1 || m_parameter <= samples.front().parameter)
        {
            return BlendPair{.first = 0, .second = 0, .weight = 0.0f};
        }

        if (m_parameter >= samples.back().parameter)
        {
            return BlendPair{.first = last, .second = last, .weight = 0.0f};
        }

        uint32_t first = 0;

        while (first + 1 < last && samples[first + 1].parameter <= m_parameter)
        {
            ++first;
        }

        const float low = samples[first].parameter;
        const float high = samples[first + 1].parameter;
        const float weight = high > low ? (m_parameter - low) / (high - low) : 0.0f;

        return BlendPair{.first = first, .second = first + 1, .weight = weight};
    }

    float Animator::duration(const Motion& motion, std::span<const AnimationClip> clips) const
    {
        const BlendPair pair = blendPair(motion);

        return std::lerp(clipDuration(clips, motion.samples[pair.first].clip),
            clipDuration(clips, motion.samples[pair.second].clip), pair.weight);
    }

    void Animator::advanceMotion(Motion& motion, std::span<const AnimationClip> clips, float deltaSeconds) const
    {
        const float length = duration(motion, clips);

        if (length <= 0.0f)
        {
            motion.phase = 0.0f;
            return;
        }

        const float phase = motion.phase + deltaSeconds * motion.options.speed / length;
        motion.phase = motion.options.loop ? phase - std::floor(phase) : std::clamp(phase, 0.0f, 1.0f);
    }

    void Animator::evaluateMotion(const Motion& motion, const Skeleton& skeleton, std::span<const AnimationClip> clips,
        Pose& pose)
    {
        const BlendPair pair = blendPair(motion);
        const uint32_t first = motion.samples[pair.first].clip;
        const uint32_t second = motion.samples[pair.second].clip;
        resetToRest(skeleton, pose);

        if (first < clips.size())
        {
            sampleClip(clips[first], motion.phase * clips[first].duration, pose);
        }

        if (pair.weight <= 0.0f || second == first || second >= clips.size())
        {
            return;
        }

        resetToRest(skeleton, m_scratch);
        sampleClip(clips[second], motion.phase * clips[second].duration, m_scratch);
        blendPoses(pose, m_scratch, pair.weight, pose);
    }

    void Animator::start(Motion&& motion, float fadeSeconds)
    {
        if (m_hasCurrent && fadeSeconds > 0.0f)
        {
            m_previous = std::move(m_current);
            m_hasPrevious = true;
            m_fadeSeconds = fadeSeconds;
            m_fadeElapsed = 0.0f;
        }
        else
        {
            m_hasPrevious = false;
            m_previous = Motion{};
        }

        m_current = std::move(motion);
        m_hasCurrent = true;
    }
} //namespace lockstep::anim
