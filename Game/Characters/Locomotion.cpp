#include "Characters/Locomotion.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <string>
#include <vector>

namespace game
{
    namespace
    {
        namespace anim = lockstep::anim;

        using math::k_pi;

        constexpr float k_crossfadeSeconds{0.2f};
        constexpr float k_velocitySmoothingSeconds{0.08f};
        constexpr float k_yawSmoothingSeconds{0.1f};
        constexpr float k_teleportSpeed{20.0f};
        constexpr float k_startSpeed{0.45f};
        constexpr float k_stopSpeed{0.25f};
        constexpr float k_startTurnRate{1.5f};
        constexpr float k_stopTurnRate{0.8f};
        constexpr float k_sectorHalfAngle{k_pi * 0.25f};
        constexpr float k_sectorHysteresis{k_pi / 15.0f};
        constexpr float k_minPlayback{0.5f};
        constexpr float k_maxPlayback{2.8f};
        constexpr float k_walkOnlyStretch{1.15f};
        constexpr uint32_t k_strideSamples{96};
        constexpr float k_plantedHeight{0.03f};
        constexpr float k_minStrideSpeed{0.1f};
        constexpr std::string_view k_leftFoot{"LeftFoot"};
        constexpr std::string_view k_rightFoot{"RightFoot"};
        constexpr std::string_view k_hips{"Hips"};

        constexpr float k_fallbackWalk{1.4f};
        constexpr float k_fallbackRun{3.7f};
        constexpr float k_fallbackSide{1.3f};

        bool endsWith(std::string_view text, std::string_view suffix)
        {
            return text.size() >= suffix.size() && text.substr(text.size() - suffix.size()) == suffix;
        }

        uint32_t findJointEndingWith(const anim::Skeleton& skeleton, std::string_view suffix)
        {
            for (uint32_t joint = 0; joint < skeleton.joints.size(); ++joint)
            {
                if (endsWith(skeleton.joints[joint].name, suffix))
                {
                    return joint;
                }
            }

            return anim::k_noJoint;
        }

        math::Vec3 jointPosition(const math::Mat4& matrix)
        {
            const math::Vec4 column = matrix.getColumn(3);

            return math::Vec3{column.x, column.y, column.z};
        }

        void appendPlantedSpeeds(const std::vector<math::Vec3>& positions, float sampleSeconds,
            std::vector<float>& speeds)
        {
            float lowest = positions.front().y;

            for (const math::Vec3& position : positions)
            {
                lowest = std::min(lowest, position.y);
            }

            for (std::size_t sample = 0; sample < positions.size(); ++sample)
            {
                const math::Vec3& from = positions[sample];
                const math::Vec3& to = positions[(sample + 1) % positions.size()];

                if (from.y > lowest + k_plantedHeight || to.y > lowest + k_plantedHeight)
                {
                    continue;
                }

                const math::Vec3 step{to.x - from.x, 0.0f, to.z - from.z};
                speeds.push_back(math::length(step) / sampleSeconds);
            }
        }

        float measureStrideSpeed(const anim::SkinnedModel& model, uint32_t clip, float fallback)
        {
            const anim::Skeleton& skeleton = model.skeleton;
            const std::array<uint32_t, 2> feet{findJointEndingWith(skeleton, k_leftFoot),
                findJointEndingWith(skeleton, k_rightFoot)};
            const anim::AnimationClip& animation = model.clips[clip];

            if (feet[0] == anim::k_noJoint || feet[1] == anim::k_noJoint || animation.duration <= 0.0f)
            {
                return fallback;
            }

            std::array<std::vector<math::Vec3>, 2> positions;
            std::vector<math::Mat4> matrices(skeleton.joints.size(), math::Mat4{1.0f});
            anim::Pose pose;
            const float sampleSeconds = animation.duration / static_cast<float>(k_strideSamples);

            for (uint32_t sample = 0; sample < k_strideSamples; ++sample)
            {
                anim::resetToRest(skeleton, pose);
                anim::sampleClip(animation, sampleSeconds * static_cast<float>(sample), pose);
                anim::computeJointMatrices(skeleton, pose, matrices);

                for (std::size_t foot = 0; foot < feet.size(); ++foot)
                {
                    positions[foot].push_back(jointPosition(matrices[feet[foot]]));
                }
            }

            std::vector<float> speeds;

            for (const std::vector<math::Vec3>& footPositions : positions)
            {
                appendPlantedSpeeds(footPositions, sampleSeconds, speeds);
            }

            if (speeds.empty())
            {
                return fallback;
            }

            const auto middle = speeds.begin() + static_cast<std::ptrdiff_t>(speeds.size() / 2);
            std::nth_element(speeds.begin(), middle, speeds.end());

            return *middle > k_minStrideSpeed ? *middle : fallback;
        }

        std::optional<uint32_t> findOptionalClip(const anim::SkinnedModel& model, std::string_view name)
        {
            if (name.empty())
            {
                return std::nullopt;
            }

            return anim::findClip(model.clips, name);
        }

        float wrapAngle(float angle)
        {
            return std::remainder(angle, 2.0f * k_pi);
        }

        float velocityAngle(const math::Vec3& velocity, float yaw)
        {
            const math::Vec3 forward{-std::sin(yaw), 0.0f, -std::cos(yaw)};
            const math::Vec3 right{std::cos(yaw), 0.0f, -std::sin(yaw)};
            return std::atan2(math::dot(velocity, right), math::dot(velocity, forward));
        }

        float smoothingWeight(float deltaSeconds, float smoothingSeconds)
        {
            return 1.0f - std::exp(-deltaSeconds / smoothingSeconds);
        }

        bool isMovingGait(Gait gait)
        {
            return gait == Gait::Forward || gait == Gait::Backward || gait == Gait::StrafeLeft
                || gait == Gait::StrafeRight;
        }

        bool isTurningGait(Gait gait)
        {
            return gait == Gait::TurnLeft || gait == Gait::TurnRight;
        }

        float sectorCentre(Gait gait)
        {
            switch (gait)
            {
                case Gait::Backward:
                    return k_pi;
                case Gait::StrafeLeft:
                    return -k_pi * 0.5f;
                case Gait::StrafeRight:
                    return k_pi * 0.5f;
                default:
                    return 0.0f;
            }
        }

        bool isInSector(Gait gait, float angle, float halfAngle)
        {
            return std::abs(wrapAngle(angle - sectorCentre(gait))) <= halfAngle;
        }

        Gait movingGait(Gait current, float angle)
        {
            if (isMovingGait(current) && isInSector(current, angle, k_sectorHalfAngle + k_sectorHysteresis))
            {
                return current;
            }

            for (const Gait gait : {Gait::Forward, Gait::Backward, Gait::StrafeLeft, Gait::StrafeRight})
            {
                if (isInSector(gait, angle, k_sectorHalfAngle))
                {
                    return gait;
                }
            }

            return Gait::Forward;
        }

        Gait pickGait(const LocomotionState& state, const LocomotionSet& set, float yaw, bool isMoving,
            const LocomotionTuning& tuning)
        {
            if (isMoving)
            {
                return movingGait(state.gait, velocityAngle(state.velocity, yaw));
            }

            if (tuning.isTurningInPlace)
            {
                const std::optional<TurnStep>& step = state.signals.step;

                if (!step || !step->clip)
                {
                    return Gait::Idle;
                }

                return step->isLeft ? Gait::TurnLeft : Gait::TurnRight;
            }

            const bool isTurning = std::abs(state.yawRate) > (isTurningGait(state.gait) ? k_stopTurnRate : k_startTurnRate);

            if (isTurning && state.yawRate > 0.0f && set.turnLeft)
            {
                return Gait::TurnLeft;
            }

            if (isTurning && state.yawRate < 0.0f && set.turnRight)
            {
                return Gait::TurnRight;
            }

            return Gait::Idle;
        }

        struct GaitPlay
        {
            const LocomotionSet& set;
            const LocomotionTuning& tuning;
            float speed{0.0f};
        };

        float playbackFor(const GaitPlay& play, float strideSpeed, LocomotionSignals& signals)
        {
            return stridePlayback(play.tuning, play.speed, strideSpeed, k_minPlayback, k_maxPlayback,
                signals.strideScale);
        }

        void playStride(anim::Animator& animator, const GaitPlay& play, const StrideClip& stride,
            LocomotionSignals& signals)
        {
            animator.play(stride.clip, k_crossfadeSeconds,
                {.loop = true, .speed = stride.direction * playbackFor(play, stride.strideSpeed, signals)});
        }

        float forwardBlendParameter(const LocomotionSet& set, float speed)
        {
            const float walkTop = set.walk.strideSpeed * k_walkOnlyStretch;

            if (speed <= walkTop || set.run.strideSpeed <= walkTop)
            {
                return set.walk.strideSpeed;
            }

            const float runAmount = std::min((speed - walkTop) / (set.run.strideSpeed - walkTop), 1.0f);

            return set.walk.strideSpeed + (set.run.strideSpeed - set.walk.strideSpeed) * runAmount;
        }

        void playForward(anim::Animator& animator, const GaitPlay& play, LocomotionSignals& signals)
        {
            const LocomotionSet& set = play.set;
            const std::array<anim::BlendSample, 2> samples{{{.clip = set.walk.clip, .parameter = set.walk.strideSpeed},
                {.clip = set.run.clip, .parameter = set.run.strideSpeed}}};
            const float parameter = forwardBlendParameter(set, play.speed);
            const float playback = play.speed > set.run.strideSpeed ? playbackFor(play, set.run.strideSpeed, signals)
                                                                    : playbackFor(play, parameter, signals);

            animator.playBlend(samples, k_crossfadeSeconds, {.loop = true, .speed = playback});
            animator.setParameter(parameter);
        }

        void playTurn(anim::Animator& animator, const GaitPlay& play, std::optional<uint32_t> clip,
            const std::optional<TurnStep>& step)
        {
            if (!play.tuning.isTurningInPlace || !step || !step->clip)
            {
                animator.play(clip.value_or(play.set.idle), k_crossfadeSeconds);
                return;
            }

            animator.play(step->clip->clip, k_crossfadeSeconds,
                {.loop = false, .speed = play.tuning.turnClipRate, .removeRootYaw = true,
                    .startPhase = step->clip->startPhase});
        }

        void playGait(anim::Animator& animator, const GaitPlay& play, Gait gait, LocomotionSignals& signals)
        {
            signals.strideScale = 1.0f;

            switch (gait)
            {
                case Gait::Idle:
                    animator.play(play.set.idle, k_crossfadeSeconds);
                    break;
                case Gait::Forward:
                    playForward(animator, play, signals);
                    break;
                case Gait::Backward:
                    playStride(animator, play, play.set.walkBack, signals);
                    break;
                case Gait::StrafeLeft:
                    playStride(animator, play, play.set.strafeLeft, signals);
                    break;
                case Gait::StrafeRight:
                    playStride(animator, play, play.set.strafeRight, signals);
                    break;
                case Gait::TurnLeft:
                    playTurn(animator, play, play.set.turnLeft, signals.step);
                    break;
                case Gait::TurnRight:
                    playTurn(animator, play, play.set.turnRight, signals.step);
                    break;
            }
        }

        float warpTarget(const LocomotionState& state, float yaw)
        {
            if (!isMovingGait(state.gait))
            {
                return 0.0f;
            }

            return wrapAngle(sectorCentre(state.gait) - velocityAngle(state.velocity, yaw));
        }
    } //namespace

    lockstep::Result<LocomotionSet> findLocomotionSet(const anim::SkinnedModel& model, const LocomotionClipNames& names)
    {
        const std::array<std::string_view, 6> required{names.idle, names.walk, names.run, names.walkBack,
            names.strafeLeft, names.strafeRight};
        std::array<uint32_t, 6> clips{};

        for (std::size_t index = 0; index < required.size(); ++index)
        {
            const std::optional<uint32_t> clip = anim::findClip(model.clips, required[index]);

            if (!clip)
            {
                return lockstep::Error{lockstep::ErrorCode::InvalidArgument,
                    "character '" + model.name + "' has no clip '" + std::string(required[index]) + "'"};
            }

            clips[index] = *clip;
        }

        const uint32_t hips = findJointEndingWith(model.skeleton, k_hips);

        return LocomotionSet{
            .idle = clips[0],
            .walk = {clips[1], measureStrideSpeed(model, clips[1], k_fallbackWalk)},
            .run = {clips[2], measureStrideSpeed(model, clips[2], k_fallbackRun)},
            .walkBack = {clips[3], measureStrideSpeed(model, clips[3], k_fallbackSide), names.reverseWalkBack ? -1.0f : 1.0f},
            .strafeLeft = {clips[4], measureStrideSpeed(model, clips[4], k_fallbackSide)},
            .strafeRight = {clips[5], measureStrideSpeed(model, clips[5], k_fallbackSide)},
            .turnLeft = findOptionalClip(model, names.turnLeft),
            .turnRight = findOptionalClip(model, names.turnRight),
            .turnClips = {.left = measureTurnClip(model, hips, findOptionalClip(model, names.turnLeft)),
                .right = measureTurnClip(model, hips, findOptionalClip(model, names.turnRight))}};
    }

    void updateLocomotion(LocomotionState& state, const LocomotionSet& set, anim::Animator& animator,
        const math::Vec3& foot, float yaw, float deltaSeconds, const LocomotionTuning& tuning,
        const anim::RootJoint& root)
    {
        animator.setInertialization(tuning.isInertialized ? tuning.halflifePerFade : 0.0f);
        animator.setRootJoint(root);

        const bool isFirst = !state.hasLast;
        state.stance = -1;

        if (!observeLocomotion(state, foot, yaw, deltaSeconds))
        {
            if (isFirst)
            {
                animator.play(set.idle, 0.0f);
            }

            return;
        }

        const float speed = math::length(state.velocity);
        const bool isMoving = speed > (isMovingGait(state.gait) ? k_stopSpeed : k_startSpeed);
        const bool isTurnFree = std::abs(state.yawRate) < k_stopTurnRate;
        updateTurnInPlace(state.signals, tuning, yaw, isMoving, isTurnFree, set.turnClips, deltaSeconds);
        state.gait = pickGait(state, set, yaw, isMoving, tuning);
        playGait(animator, GaitPlay{.set = set, .tuning = tuning, .speed = speed}, state.gait, state.signals);
        updateLocomotionSignals(state.signals, tuning, state.velocity, yaw, warpTarget(state, yaw), deltaSeconds);
    }

    bool observeLocomotion(LocomotionState& state, const math::Vec3& foot, float yaw, float deltaSeconds)
    {
        if (!state.hasLast)
        {
            state = LocomotionState{.lastFoot = foot, .lastYaw = yaw, .hasLast = true};
            return false;
        }

        if (deltaSeconds <= 0.0f)
        {
            return false;
        }

        const float climbed = (foot.y - state.lastFoot.y) / deltaSeconds;
        const math::Vec3 moved{foot.x - state.lastFoot.x, 0.0f, foot.z - state.lastFoot.z};
        const math::Vec3 velocity = moved * (1.0f / deltaSeconds);
        const float yawRate = wrapAngle(yaw - state.lastYaw) / deltaSeconds;
        state.lastFoot = foot;
        state.lastYaw = yaw;

        if (math::length(velocity) > k_teleportSpeed || std::abs(climbed) > k_teleportSpeed)
        {
            state.velocity = math::Vec3{0.0f, 0.0f, 0.0f};
            state.yawRate = 0.0f;
            state.verticalSpeed = 0.0f;
            state.signals.lastVelocity = state.velocity;
        }
        else
        {
            const float weight = smoothingWeight(deltaSeconds, k_velocitySmoothingSeconds);
            state.velocity = math::lerp(state.velocity, velocity, weight);
            state.verticalSpeed += (climbed - state.verticalSpeed) * weight;
            state.yawRate += (yawRate - state.yawRate) * smoothingWeight(deltaSeconds, k_yawSmoothingSeconds);
        }

        return true;
    }

    const char* gaitName(Gait gait)
    {
        switch (gait)
        {
            case Gait::Idle:
                return "idle";
            case Gait::Forward:
                return "forward";
            case Gait::Backward:
                return "backward";
            case Gait::StrafeLeft:
                return "strafe left";
            case Gait::StrafeRight:
                return "strafe right";
            case Gait::TurnLeft:
                return "turn left";
            case Gait::TurnRight:
                return "turn right";
        }

        return "?";
    }
} //namespace game
