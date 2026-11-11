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

        math::Quat parentRotation(const Skeleton& skeleton, const Pose& pose, uint32_t joint)
        {
            math::Quat rotation{};
            uint32_t parent = skeleton.joints[joint].parent;

            while (parent < skeleton.joints.size())
            {
                rotation = pose.joints[parent].rotation * rotation;
                parent = skeleton.joints[parent].parent;
            }

            return rotation;
        }

        math::Transform blendTransforms(const math::Transform& from, const math::Transform& to, float weight)
        {
            return math::Transform{.position = math::lerp(from.position, to.position, weight),
                .rotation = math::slerp(from.rotation, to.rotation, weight),
                .scale = math::lerp(from.scale, to.scale, weight)};
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
        motion.phase = std::clamp(options.startPhase, 0.0f, 1.0f);
        std::stable_sort(motion.samples.begin(), motion.samples.end(),
            [](const BlendSample& a, const BlendSample& b) { return a.parameter < b.parameter; });

        if (m_hasCurrent && sameSamples(m_current.samples, motion.samples))
        {
            m_current.options = options;
            return;
        }

        start(std::move(motion), fadeSeconds);
    }

    void Animator::playFrom(uint32_t clip, float startPhase, float fadeSeconds, const PlaybackOptions& options)
    {
        Motion motion{};
        motion.samples.push_back(BlendSample{.clip = clip, .parameter = 0.0f});
        motion.options = options;
        motion.phase = std::clamp(startPhase, 0.0f, 1.0f);
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

    void Animator::playLayer(uint32_t clip, const BoneMask& mask, float fadeSeconds, const PlaybackOptions& options)
    {
        m_layerMask = mask;
        m_layerTarget = 1.0f;
        m_layerFadeSeconds = std::max(fadeSeconds, 0.0f);
        const bool isSameClip = m_hasLayer && m_layer.samples.size() == 1 && m_layer.samples.front().clip == clip;

        if (isSameClip)
        {
            m_layer.options = options;
            return;
        }

        Motion motion{};
        motion.samples.push_back(BlendSample{.clip = clip, .parameter = 0.0f});
        motion.options = options;

        if (m_hasLayer && m_layerWeight > 0.0f && fadeSeconds > 0.0f)
        {
            m_previousLayer = std::move(m_layer);
            m_hasPreviousLayer = true;
            m_layerBlendSeconds = fadeSeconds;
            m_layerBlendElapsed = 0.0f;
        }

        m_layer = std::move(motion);
        m_hasLayer = true;

        if (fadeSeconds <= 0.0f)
        {
            m_layerWeight = 1.0f;
        }
    }

    void Animator::stopLayer(float fadeSeconds)
    {
        m_layerTarget = 0.0f;
        m_layerFadeSeconds = std::max(fadeSeconds, 0.0f);

        if (fadeSeconds <= 0.0f)
        {
            m_layerWeight = 0.0f;
            m_hasLayer = false;
            m_hasPreviousLayer = false;
        }
    }

    float Animator::layerWeight() const noexcept
    {
        return m_hasLayer ? m_layerWeight : 0.0f;
    }

    void Animator::advance(std::span<const AnimationClip> clips, float deltaSeconds)
    {
        const float delta = std::max(deltaSeconds, 0.0f);
        m_unevaluatedSeconds += delta;
        advanceLayer(clips, delta);

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
        const float delta = std::exchange(m_unevaluatedSeconds, 0.0f);

        if (!m_hasCurrent)
        {
            resetToRest(skeleton, pose);
            return;
        }

        evaluateMotion(m_current, skeleton, clips, pose);

        if (m_hasPrevious && m_fadeSeconds > 0.0f)
        {
            evaluateMotion(m_previous, skeleton, clips, m_previousPose);
            blendPoses(m_previousPose, pose, m_fadeElapsed / m_fadeSeconds, pose);
        }

        if (m_hasPendingInertialization)
        {
            beginInertialization(skeleton, clips, pose, delta);
        }

        m_inertializer.apply(pose, delta);
        applyLayer(skeleton, clips, pose);
    }

    void Animator::setInertialization(float halflifePerFadeSecond)
    {
        m_inertialScale = std::max(halflifePerFadeSecond, 0.0f);
    }

    float Animator::inertialization() const noexcept
    {
        return m_inertialScale;
    }

    void Animator::beginInertialization(const Skeleton& skeleton, std::span<const AnimationClip> clips, const Pose& pose,
        float deltaSeconds)
    {
        m_hasPendingInertialization = false;
        const float phase = m_current.phase;
        advanceMotion(m_current, clips, -deltaSeconds);
        evaluateMotion(m_current, skeleton, clips, m_inertialBefore);
        m_current.phase = phase;
        m_inertializer.begin(pose, m_inertialBefore, deltaSeconds, m_pendingHalflife);
    }

    void Animator::advanceLayer(std::span<const AnimationClip> clips, float deltaSeconds)
    {
        if (!m_hasLayer)
        {
            return;
        }

        const float step = m_layerFadeSeconds > 0.0f ? deltaSeconds / m_layerFadeSeconds : 1.0f;
        m_layerWeight = m_layerTarget > m_layerWeight ? std::min(m_layerWeight + step, m_layerTarget)
                                                      : std::max(m_layerWeight - step, m_layerTarget);
        advanceMotion(m_layer, clips, deltaSeconds);

        if (m_hasPreviousLayer)
        {
            advanceMotion(m_previousLayer, clips, deltaSeconds);
            m_layerBlendElapsed += deltaSeconds;
            m_hasPreviousLayer = m_layerBlendElapsed < m_layerBlendSeconds;
        }

        if (m_layerTarget <= 0.0f && m_layerWeight <= 0.0f)
        {
            m_hasLayer = false;
            m_hasPreviousLayer = false;
        }
    }

    void Animator::applyLayer(const Skeleton& skeleton, std::span<const AnimationClip> clips, Pose& pose)
    {
        if (!m_hasLayer || m_layerWeight <= 0.0f || pose.joints.size() != skeleton.joints.size())
        {
            return;
        }

        evaluateMotion(m_layer, skeleton, clips, m_layerPose);

        if (m_hasPreviousLayer && m_layerBlendSeconds > 0.0f)
        {
            evaluateMotion(m_previousLayer, skeleton, clips, m_previousLayerPose);
            blendPoses(m_previousLayerPose, m_layerPose, m_layerBlendElapsed / m_layerBlendSeconds, m_layerPose);
        }


        for (const uint32_t root : m_layerMask.modelSpaceRoots)
        {
            if (root >= pose.joints.size())
            {
                continue;
            }

            const math::Quat baseParent = parentRotation(skeleton, pose, root);
            const math::Quat layerParent = parentRotation(skeleton, m_layerPose, root);
            m_layerPose.joints[root].rotation = math::normalize(math::inverse(baseParent) * layerParent
                * m_layerPose.joints[root].rotation);
        }

        const std::size_t count = std::min(m_layerMask.weights.size(), pose.joints.size());

        for (std::size_t index = 0; index < count; ++index)
        {
            const float weight = m_layerMask.weights[index] * m_layerWeight;

            if (weight > 0.0f)
            {
                pose.joints[index] = blendTransforms(pose.joints[index], m_layerPose.joints[index], weight);
            }
        }
    }

    bool Animator::isFading() const noexcept
    {
        return m_hasPrevious || m_hasPendingInertialization || m_inertializer.isActive();
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
            sampleMotionClip(motion, clips[first], pose);
        }

        if (pair.weight <= 0.0f || second == first || second >= clips.size())
        {
            return;
        }

        resetToRest(skeleton, m_scratch);
        sampleMotionClip(motion, clips[second], m_scratch);
        blendPoses(pose, m_scratch, pair.weight, pose);
    }

    void Animator::sampleMotionClip(const Motion& motion, const AnimationClip& clip, Pose& pose) const
    {
        const float seconds = motion.phase * clip.duration;
        sampleClip(clip, seconds, pose);

        if (motion.options.removeRootYaw)
        {
            turnRoot(pose, m_root, -clipRootYaw(clip, m_root, seconds));
        }
    }

    void Animator::setRootJoint(const RootJoint& root)
    {
        m_root = root;
    }

    void Animator::start(Motion&& motion, float fadeSeconds)
    {
        const bool isInertial = m_inertialScale > 0.0f && m_hasCurrent && fadeSeconds > 0.0f;
        m_hasPendingInertialization = isInertial;
        m_pendingHalflife = fadeSeconds * m_inertialScale;

        if (fadeSeconds <= 0.0f)
        {
            m_inertializer.reset();
        }

        if (m_hasCurrent && fadeSeconds > 0.0f && !isInertial)
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
