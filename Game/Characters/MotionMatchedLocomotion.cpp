#include "Characters/MotionMatchedLocomotion.hpp"

#include <algorithm>
#include <cmath>

namespace game
{
    namespace
    {
        namespace anim = lockstep::anim;

        constexpr float k_maxWantedSpeed{7.0f};
        constexpr float k_standingSpeed{0.2f};
        constexpr float k_standingWanted{0.35f};
        constexpr float k_maxWantedTurn{1.6f};
        constexpr float k_suddenChange{1.0f};
        constexpr float k_movingSpeed{0.25f};
        constexpr float k_minStrideSpeed{0.3f};
        constexpr float k_minWarpSpeed{0.4f};

        math::Vec3 toCharacter(const math::Vec3& world, float yaw)
        {
            const math::Vec3 forward{-std::sin(yaw), 0.0f, -std::cos(yaw)};
            const math::Vec3 left{-std::cos(yaw), 0.0f, std::sin(yaw)};
            return math::Vec3{math::dot(world, left), 0.0f, math::dot(world, forward)};
        }

        math::Vec3 wantedVelocity(const math::Vec3& velocity, const math::Vec3& acceleration, float anticipation)
        {
            math::Vec3 wanted = velocity + acceleration * anticipation;
            const float speed = math::length(wanted);

            if (speed > k_maxWantedSpeed)
            {
                wanted = wanted * (k_maxWantedSpeed / speed);
            }

            const bool isStanding = math::length(velocity) < k_standingSpeed && math::length(wanted) < k_standingWanted;
            return isStanding ? math::Vec3{0.0f, 0.0f, 0.0f} : wanted;
        }

        float headingOf(const math::Vec3& velocity)
        {
            return std::atan2(velocity.x, velocity.z);
        }

        void setWarpSignals(LocomotionState& state, const LocomotionTuning& tuning, const math::Vec3& velocity,
            const math::Vec3& rootVelocity, float yaw, float deltaSeconds)
        {
            const float speed = math::length(velocity);
            const float clipSpeed = math::length(math::Vec3{rootVelocity.x, 0.0f, rootVelocity.z});
            float warp = 0.0f;

            if (tuning.isOrientationWarped && speed > k_minWarpSpeed && clipSpeed > k_minWarpSpeed)
            {
                warp = std::clamp(std::remainder(headingOf(velocity) - headingOf(rootVelocity), 2.0f * math::k_pi),
                    -tuning.maxMatchWarp, tuning.maxMatchWarp);
            }

            LocomotionTuning withoutTurns = tuning;
            withoutTurns.isTurningInPlace = false;
            updateTurnInPlace(state.signals, withoutTurns, yaw, true, false, TurnClips{}, deltaSeconds);
            updateLocomotionSignals(state.signals, tuning, state.velocity, yaw, warp, deltaSeconds);
            const bool isStrided = tuning.isStrideWarped && speed > k_minStrideSpeed && clipSpeed > k_minStrideSpeed;
            state.signals.strideScale = isStrided ? speed / clipSpeed : 1.0f;
        }
    } //namespace

    bool updateMotionMatchedLocomotion(LocomotionState& state, const MotionSets& sets, MotionStance stance,
        anim::Animator& animator, const math::Vec3& foot, float yaw, float deltaSeconds, const LocomotionTuning& tuning)
    {
        const anim::MotionDatabase* database = motionDatabase(sets, stance);

        if (database == nullptr)
        {
            return false;
        }

        animator.setInertialization(tuning.isInertialized ? tuning.halflifePerFade : 0.0f);
        const bool isFirst = !state.hasLast;

        if (!observeLocomotion(state, foot, yaw, deltaSeconds) && !isFirst)
        {
            return true;
        }

        const auto stanceIndex = static_cast<int32_t>(stance);
        const bool isNewStance = state.stance != stanceIndex;

        if (isNewStance)
        {
            state.matcher = anim::MotionMatchState{};
            state.stance = stanceIndex;
        }

        const math::Vec3 velocity = toCharacter(state.velocity, yaw);
        const math::Vec3 wanted = wantedVelocity(velocity, toCharacter(state.signals.acceleration, yaw),
            tuning.anticipation);
        const float wantedYaw = std::clamp(state.yawRate * tuning.turnAnticipation, -k_maxWantedTurn, k_maxWantedTurn);
        const anim::MotionTrajectory trajectory = anim::predictTrajectory(velocity, wanted, wantedYaw,
            tuning.trajectoryHalflife);
        const bool isSudden = math::length(wanted - state.wanted) > k_suddenChange;
        state.wanted = wanted;
        const float delta = isFirst ? 0.0f : deltaSeconds;
        const anim::MotionMatchResult result = anim::updateMotionMatching(*database, state.matcher, trajectory,
            tuning.matching, delta, isNewStance || isSudden);

        if (result.isJump)
        {
            animator.playFrom(result.playClip, result.startPhase, isFirst ? 0.0f : tuning.matchFade,
                anim::PlaybackOptions{.loop = result.isLooping});
        }

        state.gait = math::length(velocity) > k_movingSpeed ? Gait::Forward : Gait::Idle;
        setWarpSignals(state, tuning, velocity, result.rootVelocity, yaw, delta);
        return true;
    }
} //namespace game
