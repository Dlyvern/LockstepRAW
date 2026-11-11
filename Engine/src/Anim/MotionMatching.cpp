#include "Lockstep/Anim/MotionMatching.hpp"

#include "Lockstep/Core/Spring.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace lockstep::anim
{
    namespace
    {
        using FeatureVector = std::array<float, k_motionFeatureCount>;

        float distance(std::span<const float> features, const FeatureVector& query, float limit)
        {
            float cost = 0.0f;

            for (std::size_t dimension = 0; dimension < k_motionFeatureCount && cost < limit; ++dimension)
            {
                const float difference = features[dimension] - query[dimension];
                cost += difference * difference;
            }

            return cost;
        }

        struct Candidate
        {
            uint32_t frame{0};
            float cost{std::numeric_limits<float>::max()};
        };

        Candidate search(const MotionDatabase& database, const FeatureVector& query)
        {
            Candidate best{};

            for (uint32_t frame = 0; frame < database.frameCount(); ++frame)
            {
                if (!database.isSearchable(frame))
                {
                    continue;
                }

                const float cost = distance(database.features(frame), query, best.cost);

                if (cost < best.cost)
                {
                    best = Candidate{.frame = frame, .cost = cost};
                }
            }

            return best;
        }

        bool advance(const MotionDatabase& database, MotionMatchState& state, float deltaSeconds)
        {
            if (state.clip == k_noMotionClip)
            {
                return true;
            }

            const MotionDatabaseClip& clip = database.clips()[state.clip];
            state.seconds += deltaSeconds;

            if (clip.isLooping)
            {
                state.seconds = clip.duration > 0.0f ? std::fmod(state.seconds, clip.duration) : 0.0f;
                return false;
            }

            const bool isAtEnd = state.seconds >= clip.duration - 1.0f / k_motionSampleRate;
            state.seconds = std::min(state.seconds, clip.duration);
            return isAtEnd;
        }

        FeatureVector makeQuery(const MotionDatabase& database, const MotionMatchState& state,
            const MotionTrajectory& wanted)
        {
            FeatureVector query{};

            if (state.clip != k_noMotionClip)
            {
                const std::span<const float> current = database.features(database.frameAt(state.clip, state.seconds));
                std::copy_n(current.begin(), k_motionTrajectoryOffset, query.begin());
            }

            database.normalizeTrajectory(wanted, query);
            return query;
        }

        bool shouldJump(const MotionDatabase& database, const MotionMatchState& state, const Candidate& best,
            float currentCost, const MotionMatchSettings& settings)
        {
            if (state.clip == k_noMotionClip)
            {
                return true;
            }

            const uint32_t clip = database.clipOf(best.frame);
            const float seconds = static_cast<float>(best.frame - database.clips()[clip].firstFrame) / k_motionSampleRate;
            const bool isNearby = clip == state.clip && std::abs(seconds - state.seconds) < settings.sameClipWindow;
            return !isNearby && best.cost < currentCost - settings.minImprovement;
        }
    } //namespace

    MotionMatchResult updateMotionMatching(const MotionDatabase& database, MotionMatchState& state,
        const MotionTrajectory& wanted, const MotionMatchSettings& settings, float deltaSeconds, bool forceSearch)
    {
        MotionMatchResult result{};

        if (database.frameCount() == 0)
        {
            return result;
        }

        const float delta = std::max(deltaSeconds, 0.0f);
        const bool isAtEnd = advance(database, state, delta);
        state.sinceSearch += delta;

        if (forceSearch || isAtEnd || state.sinceSearch >= settings.searchInterval)
        {
            state.sinceSearch = 0.0f;
            const FeatureVector query = makeQuery(database, state, wanted);
            const float currentCost = state.clip == k_noMotionClip ? std::numeric_limits<float>::max()
                : distance(database.features(database.frameAt(state.clip, state.seconds)), query,
                    std::numeric_limits<float>::max());
            const Candidate best = search(database, query);

            if (best.cost < std::numeric_limits<float>::max() && shouldJump(database, state, best, currentCost, settings))
            {
                state.clip = database.clipOf(best.frame);
                state.seconds = static_cast<float>(best.frame - database.clips()[state.clip].firstFrame) / k_motionSampleRate;
                state.jumps += 1;
                result.isJump = true;
            }

            state.cost = std::min(currentCost, best.cost);
        }

        if (state.clip == k_noMotionClip)
        {
            return result;
        }

        const MotionDatabaseClip& clip = database.clips()[state.clip];
        result.playClip = clip.playClip;
        result.startPhase = clip.duration > 0.0f ? state.seconds / clip.duration : 0.0f;
        result.isLooping = clip.isLooping;
        result.rootVelocity = database.rootVelocity(database.frameAt(state.clip, state.seconds));
        return result;
    }

    MotionTrajectory predictTrajectory(const math::Vec3& velocity, const math::Vec3& wantedVelocity, float wantedYaw,
        float halflife)
    {
        constexpr float k_step{1.0f / k_motionSampleRate};
        MotionTrajectory trajectory{.velocity = math::Vec3{velocity.x, 0.0f, velocity.z}};
        math::Vec3 position{0.0f, 0.0f, 0.0f};
        math::Vec3 currentVelocity{velocity.x, 0.0f, velocity.z};
        math::Vec3 acceleration{0.0f, 0.0f, 0.0f};
        float yaw = 0.0f;
        float yawSpeed = 0.0f;
        std::size_t point = 0;

        for (uint32_t frame = 1; frame <= k_motionTrajectoryFrames.back(); ++frame)
        {
            math::criticalSpring(currentVelocity.x, acceleration.x, wantedVelocity.x, halflife, k_step);
            math::criticalSpring(currentVelocity.z, acceleration.z, wantedVelocity.z, halflife, k_step);
            math::criticalSpring(yaw, yawSpeed, wantedYaw, halflife, k_step);
            position += currentVelocity * k_step;

            if (point < k_motionTrajectoryPoints && frame == k_motionTrajectoryFrames[point])
            {
                trajectory.positions[point] = position;
                trajectory.directions[point] = math::Vec3{std::sin(yaw), 0.0f, std::cos(yaw)};
                ++point;
            }
        }

        return trajectory;
    }
} //namespace lockstep::anim
