#include "Lockstep/Anim/MotionDatabase.hpp"

#include "MotionClips.hpp"

#include <algorithm>
#include <cmath>
#include <string>

namespace lockstep::anim
{
    namespace
    {
        constexpr float k_endExclusionSeconds{0.2f};
        constexpr float k_minScale{1.0e-3f};

        enum class FeatureGroup : uint8_t
        {
            FootPosition = 0,
            FootVelocity,
            HipVelocity,
            TrajectoryPosition,
            TrajectoryDirection,
            Count
        };

        FeatureGroup groupOf(std::size_t dimension)
        {
            if (dimension < 6)
            {
                return FeatureGroup::FootPosition;
            }

            if (dimension < 12)
            {
                return FeatureGroup::FootVelocity;
            }

            if (dimension < k_motionTrajectoryOffset)
            {
                return FeatureGroup::HipVelocity;
            }

            return (dimension - k_motionTrajectoryOffset) % 4 < 2 ? FeatureGroup::TrajectoryPosition
                                                                   : FeatureGroup::TrajectoryDirection;
        }

        float weightOf(const MotionFeatureWeights& weights, FeatureGroup group)
        {
            switch (group)
            {
                case FeatureGroup::FootPosition:
                    return weights.footPosition;
                case FeatureGroup::FootVelocity:
                    return weights.footVelocity;
                case FeatureGroup::HipVelocity:
                    return weights.hipVelocity;
                case FeatureGroup::TrajectoryPosition:
                    return weights.trajectoryPosition;
                case FeatureGroup::TrajectoryDirection:
                case FeatureGroup::Count:
                    break;
            }

            return weights.trajectoryDirection;
        }

        math::Vec3 toRootSpace(const math::Vec3& point, const MotionRoot& root)
        {
            return rotateYaw(point - root.position, -root.yaw);
        }

        void writeVector(float* out, const math::Vec3& value)
        {
            out[0] = value.x;
            out[1] = value.y;
            out[2] = value.z;
        }

        void rawFeatures(const SampledMotionClip& sampled, bool isLooping, uint32_t frame, float* out)
        {
            const auto count = static_cast<uint32_t>(sampled.roots.size());
            const MotionRoot& root = sampled.roots[frame];
            const uint32_t from = frame + 1 < count ? frame : frame - 1;
            const MotionJoints& now = sampled.joints[from];
            const MotionJoints& next = sampled.joints[from + 1];
            const MotionJoints& at = sampled.joints[frame];

            for (std::size_t foot = 0; foot < 2; ++foot)
            {
                writeVector(out + foot * 3, toRootSpace(at[foot + 1], root));
                writeVector(out + 6 + foot * 3, rotateYaw((next[foot + 1] - now[foot + 1]) * k_motionSampleRate, -root.yaw));
            }

            writeVector(out + 12, rotateYaw((next[0] - now[0]) * k_motionSampleRate, -root.yaw));

            for (std::size_t point = 0; point < k_motionTrajectoryPoints; ++point)
            {
                const MotionRoot future = rootAhead(sampled.roots, frame + k_motionTrajectoryFrames[point], isLooping);
                const math::Vec3 position = toRootSpace(future.position, root);
                const float turn = future.yaw - root.yaw;
                float* slot = out + k_motionTrajectoryOffset + point * 4;
                slot[0] = position.x;
                slot[1] = position.z;
                slot[2] = std::sin(turn);
                slot[3] = std::cos(turn);
            }
        }
    } //namespace

    Result<MotionDatabase> MotionDatabase::build(SkinnedModel& model, const LocomotionRig& rig,
        std::span<const MotionClipSpec> clips, const MotionFeatureWeights& weights)
    {
        if (clips.empty())
        {
            return Error{ErrorCode::InvalidArgument, "motion database needs at least one clip"};
        }

        MotionDatabase database;
        std::vector<AnimationClip> inPlace;

        for (const MotionClipSpec& spec : clips)
        {
            if (spec.clip >= model.clips.size() || model.clips[spec.clip].duration * k_motionSampleRate < 1.0f)
            {
                return Error{ErrorCode::InvalidArgument,
                    "motion database clip " + std::to_string(spec.clip) + " is unknown or too short"};
            }

            const AnimationClip& clip = model.clips[spec.clip];
            SampledMotionClip sampled = sampleMotionClip(model, rig, clip, spec.isLooping);
            const auto frames = static_cast<uint32_t>(sampled.roots.size());
            const auto first = static_cast<uint32_t>(database.m_frameClips.size());
            const auto slot = static_cast<uint32_t>(database.m_clips.size());
            const uint32_t lastSearchable = spec.isLooping ? frames
                : static_cast<uint32_t>(std::max(1.0f, static_cast<float>(frames) - k_endExclusionSeconds * k_motionSampleRate));

            for (uint32_t frame = 0; frame < frames; ++frame)
            {
                database.m_features.resize(database.m_features.size() + k_motionFeatureCount);
                rawFeatures(sampled, spec.isLooping, frame, database.m_features.data() + database.m_features.size()
                    - k_motionFeatureCount);
                database.m_frameClips.push_back(slot);
                database.m_searchable.push_back(frame < lastSearchable ? 1 : 0);
            }

            inPlace.push_back(makeInPlaceClip(model, rig.hips, clip, sampled.roots));
            database.m_clips.push_back(MotionDatabaseClip{.sourceClip = spec.clip,
                .playClip = static_cast<uint32_t>(model.clips.size() + inPlace.size() - 1),
                .isLooping = spec.isLooping,
                .duration = clip.duration,
                .firstFrame = first,
                .frameCount = frames,
                .roots = std::move(sampled.roots)});
        }

        for (AnimationClip& clip : inPlace)
        {
            model.clips.push_back(std::move(clip));
        }

        const std::size_t frameCount = database.m_frameClips.size();
        std::array<double, k_motionFeatureCount> sums{};
        std::array<double, k_motionFeatureCount> squares{};

        for (std::size_t frame = 0; frame < frameCount; ++frame)
        {
            for (std::size_t dimension = 0; dimension < k_motionFeatureCount; ++dimension)
            {
                const double value = database.m_features[frame * k_motionFeatureCount + dimension];
                sums[dimension] += value;
                squares[dimension] += value * value;
            }
        }

        constexpr auto k_groups = static_cast<std::size_t>(FeatureGroup::Count);
        std::array<double, k_groups> groupVariance{};
        std::array<double, k_groups> groupSize{};

        for (std::size_t dimension = 0; dimension < k_motionFeatureCount; ++dimension)
        {
            const double mean = sums[dimension] / static_cast<double>(frameCount);
            database.m_means[dimension] = static_cast<float>(mean);
            const auto group = static_cast<std::size_t>(groupOf(dimension));
            groupVariance[group] += std::max(squares[dimension] / static_cast<double>(frameCount) - mean * mean, 0.0);
            groupSize[group] += 1.0;
        }

        for (std::size_t dimension = 0; dimension < k_motionFeatureCount; ++dimension)
        {
            const FeatureGroup group = groupOf(dimension);
            const auto index = static_cast<std::size_t>(group);
            const float deviation = static_cast<float>(std::sqrt(groupVariance[index] / groupSize[index]));
            database.m_scales[dimension] = weightOf(weights, group) / std::max(deviation, k_minScale);
        }

        for (std::size_t frame = 0; frame < frameCount; ++frame)
        {
            for (std::size_t dimension = 0; dimension < k_motionFeatureCount; ++dimension)
            {
                float& value = database.m_features[frame * k_motionFeatureCount + dimension];
                value = (value - database.m_means[dimension]) * database.m_scales[dimension];
            }
        }

        return database;
    }

    std::span<const MotionDatabaseClip> MotionDatabase::clips() const noexcept
    {
        return m_clips;
    }

    uint32_t MotionDatabase::frameCount() const noexcept
    {
        return static_cast<uint32_t>(m_frameClips.size());
    }

    std::span<const float> MotionDatabase::features(uint32_t frame) const
    {
        return std::span<const float>(m_features).subspan(static_cast<std::size_t>(frame) * k_motionFeatureCount,
            k_motionFeatureCount);
    }

    bool MotionDatabase::isSearchable(uint32_t frame) const
    {
        return frame < m_searchable.size() && m_searchable[frame] != 0;
    }

    uint32_t MotionDatabase::clipOf(uint32_t frame) const
    {
        return frame < m_frameClips.size() ? m_frameClips[frame] : 0;
    }

    uint32_t MotionDatabase::frameAt(uint32_t clip, float seconds) const
    {
        const MotionDatabaseClip& entry = m_clips[std::min<std::size_t>(clip, m_clips.size() - 1)];
        const auto offset = static_cast<uint32_t>(std::clamp(std::lround(seconds * k_motionSampleRate), 0L,
            static_cast<long>(entry.frameCount) - 1));
        return entry.firstFrame + offset;
    }

    math::Vec3 MotionDatabase::rootVelocity(uint32_t frame) const
    {
        const MotionDatabaseClip& clip = m_clips[clipOf(frame)];
        const uint32_t local = frame - clip.firstFrame;
        const uint32_t from = local + 1 < clip.frameCount ? local : local - 1;
        const math::Vec3 step = clip.roots[from + 1].position - clip.roots[from].position;
        return rotateYaw(step * k_motionSampleRate, -clip.roots[local].yaw);
    }

    void MotionDatabase::normalizeTrajectory(const MotionTrajectory& trajectory, std::span<float> out) const
    {
        const std::array<float, 3> velocity{trajectory.velocity.x, trajectory.velocity.y, trajectory.velocity.z};

        for (std::size_t component = 0; component < velocity.size() && k_motionHipVelocityOffset + component < out.size(); ++component)
        {
            out[k_motionHipVelocityOffset + component] = (velocity[component] - m_means[k_motionHipVelocityOffset + component]) * m_scales[k_motionHipVelocityOffset + component];
        }

        for (std::size_t point = 0; point < k_motionTrajectoryPoints; ++point)
        {
            const std::size_t base = k_motionTrajectoryOffset + point * 4;
            const std::array<float, 4> raw{trajectory.positions[point].x, trajectory.positions[point].z,
                trajectory.directions[point].x, trajectory.directions[point].z};

            for (std::size_t component = 0; component < 4; ++component)
            {
                const std::size_t dimension = base + component;

                if (dimension < out.size())
                {
                    out[dimension] = (raw[component] - m_means[dimension]) * m_scales[dimension];
                }
            }
        }
    }
} //namespace lockstep::anim
