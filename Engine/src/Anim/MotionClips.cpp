#include "MotionClips.hpp"

#include "Lockstep/Anim/JointEdits.hpp"
#include "Lockstep/Core/Transform.hpp"

#include <algorithm>
#include <cmath>
#include <string>

namespace lockstep::anim
{
    namespace
    {
        constexpr math::Vec3 k_up{0.0f, 1.0f, 0.0f};
        constexpr math::Vec3 k_forward{0.0f, 0.0f, 1.0f};
        constexpr int k_smoothRadius{4};

        uint32_t sampleCount(const AnimationClip& clip)
        {
            return std::max(2u, static_cast<uint32_t>(std::lround(clip.duration * k_motionSampleRate)) + 1u);
        }

        float sampleTime(const AnimationClip& clip, uint32_t frame)
        {
            return std::min(static_cast<float>(frame) / k_motionSampleRate, clip.duration);
        }

        MotionRoot rootAt(std::span<const MotionRoot> raw, int index, bool isLooping)
        {
            const int count = static_cast<int>(raw.size());

            if (index >= 0)
            {
                return rootAhead(raw, static_cast<uint32_t>(isLooping ? index : std::min(index, count - 1)), isLooping);
            }

            if (!isLooping)
            {
                return raw.front();
            }

            const MotionRoot& first = raw.front();
            const MotionRoot& last = raw.back();
            const float turn = last.yaw - first.yaw;
            const MotionRoot& base = raw[static_cast<std::size_t>(std::max(count - 1 + index, 0))];
            return MotionRoot{.position = first.position + rotateYaw(base.position - last.position, -turn),
                .yaw = base.yaw - turn};
        }

        std::vector<MotionRoot> smoothRoots(const std::vector<MotionRoot>& raw, bool isLooping)
        {
            const int count = static_cast<int>(raw.size());
            std::vector<MotionRoot> smoothed(raw.size());

            for (int frame = 0; frame < count; ++frame)
            {
                math::Vec3 position{0.0f, 0.0f, 0.0f};
                float yaw = 0.0f;

                for (int offset = -k_smoothRadius; offset <= k_smoothRadius; ++offset)
                {
                    const MotionRoot sample = rootAt(raw, frame + offset, isLooping);
                    position += sample.position;
                    yaw += sample.yaw;
                }

                const float samples = static_cast<float>(2 * k_smoothRadius + 1);
                smoothed[static_cast<std::size_t>(frame)] = MotionRoot{.position = position / samples, .yaw = yaw / samples};
            }

            return smoothed;
        }

        AnimationChannel resampledChannel(uint32_t joint, ChannelTarget target, const std::vector<float>& times,
            std::vector<math::Vec4>&& values)
        {
            return AnimationChannel{.joint = joint,
                .target = target,
                .interpolation = Interpolation::Linear,
                .times = times,
                .values = std::move(values)};
        }
    } //namespace

    math::Vec3 rotateYaw(const math::Vec3& vector, float yaw)
    {
        const float cosine = std::cos(yaw);
        const float sine = std::sin(yaw);
        return math::Vec3{vector.x * cosine + vector.z * sine, vector.y, -vector.x * sine + vector.z * cosine};
    }

    MotionRoot rootAhead(std::span<const MotionRoot> roots, uint32_t frame, bool isLooping)
    {
        const auto count = static_cast<uint32_t>(roots.size());

        if (frame < count)
        {
            return roots[frame];
        }

        const MotionRoot& first = roots.front();
        const MotionRoot& last = roots.back();

        if (!isLooping)
        {
            const MotionRoot& before = roots[count - 2];
            const float extra = static_cast<float>(frame - (count - 1));
            return MotionRoot{.position = last.position + (last.position - before.position) * extra, .yaw = last.yaw};
        }

        const uint32_t cycle = std::max(count - 1, 1u);
        const uint32_t cycles = frame / cycle;
        const MotionRoot& base = roots[frame % cycle];
        const float turn = last.yaw - first.yaw;
        MotionRoot root{.position = base.position, .yaw = base.yaw};

        for (uint32_t index = 0; index < cycles; ++index)
        {
            root.position = last.position + rotateYaw(root.position - first.position, turn);
            root.yaw += turn;
        }

        return root;
    }

    SampledMotionClip sampleMotionClip(const SkinnedModel& model, const LocomotionRig& rig, const AnimationClip& clip,
        bool isLooping)
    {
        const Skeleton& skeleton = model.skeleton;
        const uint32_t count = sampleCount(clip);
        std::vector<math::Mat4> matrices(skeleton.joints.size(), math::Mat4{1.0f});
        Pose pose;
        resetToRest(skeleton, pose);
        computeJointMatrices(skeleton, pose, matrices);
        const math::Quat restHips = jointRotation(matrices[rig.hips]);
        std::vector<MotionRoot> raw(count);
        SampledMotionClip sampled{.roots = {}, .joints = std::vector<MotionJoints>(count)};
        float lastYaw = 0.0f;

        for (uint32_t frame = 0; frame < count; ++frame)
        {
            resetToRest(skeleton, pose);
            sampleClip(clip, sampleTime(clip, frame), pose);
            computeJointMatrices(skeleton, pose, matrices);
            const math::Vec3 hips = jointPosition(matrices[rig.hips]);
            const math::Vec3 facing = jointRotation(matrices[rig.hips]) * (math::inverse(restHips) * k_forward);
            float yaw = std::atan2(facing.x, facing.z);
            yaw = frame == 0 ? yaw : lastYaw + std::remainder(yaw - lastYaw, 2.0f * math::k_pi);
            lastYaw = yaw;
            raw[frame] = MotionRoot{.position = math::Vec3{hips.x, 0.0f, hips.z}, .yaw = yaw};
            sampled.joints[frame] = MotionJoints{hips, jointPosition(matrices[rig.legs[0].chain.end]),
                jointPosition(matrices[rig.legs[1].chain.end])};
        }

        float stanceYaw = raw.front().yaw;

        if (isLooping)
        {
            stanceYaw = 0.0f;

            for (const MotionRoot& root : raw)
            {
                stanceYaw += root.yaw / static_cast<float>(raw.size());
            }
        }

        for (MotionRoot& root : raw)
        {
            root.yaw -= stanceYaw;
        }

        sampled.roots = smoothRoots(raw, isLooping);
        return sampled;
    }

    AnimationClip makeInPlaceClip(const SkinnedModel& model, uint32_t hips, const AnimationClip& clip,
        std::span<const MotionRoot> roots)
    {
        const Skeleton& skeleton = model.skeleton;
        AnimationClip inPlace{.name = clip.name + "@inplace", .duration = clip.duration, .channels = {}};

        for (const AnimationChannel& channel : clip.channels)
        {
            const bool isHipsMotion = channel.joint == hips
                && (channel.target == ChannelTarget::Translation || channel.target == ChannelTarget::Rotation);

            if (!isHipsMotion)
            {
                inPlace.channels.push_back(channel);
            }
        }

        std::vector<math::Mat4> matrices(skeleton.joints.size(), math::Mat4{1.0f});
        Pose pose;
        std::vector<float> times(roots.size());
        std::vector<math::Vec4> positions(roots.size());
        std::vector<math::Vec4> rotations(roots.size());
        const uint32_t parent = skeleton.joints[hips].parent;

        for (std::size_t frame = 0; frame < roots.size(); ++frame)
        {
            times[frame] = sampleTime(clip, static_cast<uint32_t>(frame));
            resetToRest(skeleton, pose);
            sampleClip(clip, times[frame], pose);
            computeJointMatrices(skeleton, pose, matrices);
            const MotionRoot& root = roots[frame];
            const math::Quat unturn = math::angleAxis(-root.yaw, k_up);
            const math::Mat4 edit = math::toMatrix(math::Transform{.position = unturn * -root.position, .rotation = unturn});
            const math::Mat4& parentMatrix = parent < matrices.size() ? matrices[parent] : skeleton.rootMatrix;
            const math::Mat4 local = math::inverse(parentMatrix) * edit * matrices[hips];
            const math::Quat rotation = jointRotation(local);
            const math::Vec3 position = jointPosition(local);
            positions[frame] = math::Vec4(position, 0.0f);
            rotations[frame] = math::Vec4{rotation.x, rotation.y, rotation.z, rotation.w};
        }

        for (std::size_t frame = 1; frame < rotations.size(); ++frame)
        {
            if (math::dot(rotations[frame], rotations[frame - 1]) < 0.0f)
            {
                rotations[frame] = rotations[frame] * -1.0f;
            }
        }

        inPlace.channels.push_back(resampledChannel(hips, ChannelTarget::Translation, times, std::move(positions)));
        inPlace.channels.push_back(resampledChannel(hips, ChannelTarget::Rotation, times, std::move(rotations)));
        return inPlace;
    }
} //namespace lockstep::anim
