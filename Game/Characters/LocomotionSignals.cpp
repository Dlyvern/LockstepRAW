#include "Characters/LocomotionSignals.hpp"

#include <Lockstep/Anim/RootMotion.hpp>
#include <Lockstep/Core/Spring.hpp>

#if LOCKSTEP_DEV_TOOLS
#include <imgui.h>
#endif

#include <algorithm>
#include <cmath>

namespace game
{
    namespace
    {
        namespace anim = lockstep::anim;

        constexpr float k_followSeconds{0.15f};
        constexpr float k_accelerationSmoothing{0.12f};
        constexpr float k_lockReleaseSeconds{0.12f};
        constexpr float k_minMoveSpeed{0.1f};
        constexpr float k_minClipAngle{0.2f};
        constexpr float k_leadWindowEnd{0.6f};
        constexpr float k_trailWindowStart{0.4f};
        constexpr float k_unlockShare{0.5f};
        constexpr float k_referenceLag{1.0f};
        constexpr float k_minShuffleScale{0.7f};
        constexpr float k_maxShuffleScale{1.2f};
        constexpr uint32_t k_turnScanSamples{64};
        constexpr float k_turnScanEdge{0.02f};

        float wrapAngle(float angle)
        {
            return std::remainder(angle, 2.0f * math::k_pi);
        }

        float smoothingWeight(float deltaSeconds, float smoothingSeconds)
        {
            return smoothingSeconds > 0.0f ? 1.0f - std::exp(-deltaSeconds / smoothingSeconds) : 1.0f;
        }

        float smoothstep(float value)
        {
            const float t = std::clamp(value, 0.0f, 1.0f);
            return t * t * (3.0f - 2.0f * t);
        }

        math::Vec3 facingForward(float yaw)
        {
            return math::Vec3{-std::sin(yaw), 0.0f, -std::cos(yaw)};
        }

        math::Vec3 facingLeft(float yaw)
        {
            return math::Vec3{-std::cos(yaw), 0.0f, std::sin(yaw)};
        }

        math::Vec3 toModel(const math::Vec3& world, float yaw)
        {
            return math::Vec3{math::dot(world, facingLeft(yaw)), 0.0f, math::dot(world, facingForward(yaw))};
        }

        void releaseFeet(LocomotionSignals& signals, float deltaSeconds)
        {
            const float release = deltaSeconds / k_lockReleaseSeconds;

            for (std::size_t leg = 0; leg < anim::k_legCount; ++leg)
            {
                signals.footLift[leg] = 0.0f;
                signals.footLock[leg] = std::max(signals.footLock[leg] - release, 0.0f);
            }
        }

        void setFoot(LocomotionSignals& signals, std::size_t leg, float lift, float lock, float deltaSeconds)
        {
            signals.footLift[leg] = lift;
            const float released = signals.footLock[leg] - deltaSeconds / k_lockReleaseSeconds;
            signals.footLock[leg] = lock >= signals.footLock[leg] ? lock : std::max(lock, released);
        }

        float clipProgress(const TurnClip& clip, float t)
        {
            const float position = std::clamp(t, 0.0f, 1.0f) * static_cast<float>(k_turnCurveSamples - 1);
            const auto index = std::min(static_cast<std::size_t>(position), k_turnCurveSamples - 2);
            const float fraction = position - static_cast<float>(index);
            return std::lerp(clip.progress[index], clip.progress[index + 1], fraction);
        }

        void shuffleFeet(LocomotionSignals& signals, const LocomotionTuning& tuning, const TurnStep& step, float t,
            float deltaSeconds)
        {
            const std::size_t lead = step.isLeft ? 0 : 1;
            const std::array<float, anim::k_legCount> starts{lead == 0 ? 0.0f : k_trailWindowStart,
                lead == 1 ? 0.0f : k_trailWindowStart};
            const float window = k_leadWindowEnd;

            for (std::size_t leg = 0; leg < anim::k_legCount; ++leg)
            {
                const float u = (t - starts[leg]) / window;
                const bool isStepping = u >= 0.0f && u < 1.0f;
                const float lift = isStepping ? tuning.stepHeight * std::sin(math::k_pi * u) : 0.0f;
                const float lock = isStepping ? 1.0f - smoothstep(u / k_unlockShare) : 1.0f;
                setFoot(signals, leg, lift, lock, deltaSeconds);
            }
        }

        void startStep(LocomotionSignals& signals, const LocomotionTuning& tuning, float lag, const TurnClips& clips)
        {
            const bool isLeft = lag < 0.0f;
            const std::optional<TurnClip>& candidate = isLeft ? clips.left : clips.right;
            const bool useClip = candidate && std::abs(candidate->angle) > k_minClipAngle
                && std::abs(lag) >= tuning.turnClipShare * std::abs(candidate->angle) && candidate->duration > 0.0f;
            const float shuffle = tuning.shuffleSeconds
                * std::clamp(std::abs(lag) / k_referenceLag, k_minShuffleScale, k_maxShuffleScale);

            signals.step = TurnStep{.duration = useClip ? candidate->duration / std::max(tuning.turnClipRate, 0.1f) : shuffle,
                .startLag = lag,
                .isLeft = isLeft,
                .clip = useClip ? candidate : std::nullopt};
            signals.settleSeconds = 0.0f;
        }

        void advanceStep(LocomotionSignals& signals, const LocomotionTuning& tuning, float yaw, float deltaSeconds)
        {
            TurnStep& step = *signals.step;
            step.seconds += deltaSeconds;
            const float t = step.duration > 0.0f ? step.seconds / step.duration : 1.0f;
            const float progress = step.clip ? clipProgress(*step.clip, t) : smoothstep(t);
            signals.bodyYaw = yaw + step.startLag * (1.0f - progress);

            if (step.clip)
            {
                releaseFeet(signals, deltaSeconds);
            }
            else
            {
                shuffleFeet(signals, tuning, step, t, deltaSeconds);
            }

            if (t >= 1.0f)
            {
                signals.step.reset();
            }
        }
    } //namespace

    std::optional<TurnClip> measureTurnClip(const anim::SkinnedModel& model, uint32_t hips, std::optional<uint32_t> clip)
    {
        if (!clip || *clip >= model.clips.size())
        {
            return std::nullopt;
        }

        const anim::AnimationClip& animation = model.clips[*clip];
        const anim::RootJoint root = anim::makeRootJoint(model.skeleton, hips);
        const float angle = anim::clipRootYaw(animation, root, animation.duration);
        const auto share = [&](float phase)
        {
            const float yaw = anim::clipRootYaw(animation, root, phase * animation.duration);
            return std::abs(angle) > 0.0f ? std::clamp(yaw / angle, 0.0f, 1.0f) : phase;
        };
        float start = 0.0f;
        float end = 1.0f;

        for (uint32_t sample = 0; sample <= k_turnScanSamples; ++sample)
        {
            const float phase = static_cast<float>(sample) / static_cast<float>(k_turnScanSamples);
            const float turned = share(phase);
            start = turned <= k_turnScanEdge ? phase : start;

            if (turned >= 1.0f - k_turnScanEdge)
            {
                end = phase;
                break;
            }
        }

        end = std::max(end, start + 1.0f / static_cast<float>(k_turnScanSamples));
        TurnClip turn{.clip = *clip, .angle = angle, .startPhase = start, .duration = (end - start) * animation.duration};

        for (std::size_t sample = 0; sample < k_turnCurveSamples; ++sample)
        {
            const float t = static_cast<float>(sample) / static_cast<float>(k_turnCurveSamples - 1);
            turn.progress[sample] = share(start + t * (end - start));
        }

        turn.progress.front() = 0.0f;
        turn.progress.back() = 1.0f;
        return turn;
    }

    void updateTurnInPlace(LocomotionSignals& signals, const LocomotionTuning& tuning, float yaw, bool isMoving,
        bool isTurnFree, const TurnClips& clips, float deltaSeconds)
    {
        if (!signals.hasBodyYaw || !tuning.isTurningInPlace)
        {
            signals.bodyYaw = yaw;
            signals.hasBodyYaw = true;
            signals.step.reset();
            signals.settleSeconds = 0.0f;
            releaseFeet(signals, deltaSeconds);
            return;
        }

        if (isMoving)
        {
            signals.step.reset();
            signals.settleSeconds = 0.0f;
            signals.bodyYaw = yaw + wrapAngle(signals.bodyYaw - yaw) * (1.0f - smoothingWeight(deltaSeconds, k_followSeconds));
            releaseFeet(signals, deltaSeconds);
            return;
        }

        if (signals.step)
        {
            advanceStep(signals, tuning, yaw, deltaSeconds);
            return;
        }

        releaseFeet(signals, deltaSeconds);
        const float maxLag = tuning.pose.maxLowerBodyYaw;
        const float lag = std::clamp(wrapAngle(signals.bodyYaw - yaw), -maxLag, maxLag);
        signals.bodyYaw = yaw + lag;
        const bool isLagging = std::abs(lag) > tuning.turnSettleAngle;
        signals.settleSeconds = isLagging && isTurnFree ? signals.settleSeconds + deltaSeconds : 0.0f;

        if (std::abs(lag) > tuning.turnStartAngle || signals.settleSeconds >= tuning.turnSettleSeconds)
        {
            startStep(signals, tuning, lag, clips);
        }
    }

    void updateLocomotionSignals(LocomotionSignals& signals, const LocomotionTuning& tuning, const math::Vec3& velocity,
        float yaw, float warpTarget, float deltaSeconds)
    {
        if (deltaSeconds <= 0.0f)
        {
            return;
        }

        const math::Vec3 rawAcceleration = (velocity - signals.lastVelocity) * (1.0f / deltaSeconds);
        signals.lastVelocity = velocity;
        signals.acceleration = math::lerp(signals.acceleration, rawAcceleration,
            smoothingWeight(deltaSeconds, k_accelerationSmoothing));

        const float warp = tuning.isOrientationWarped ? warpTarget : 0.0f;
        signals.warpYaw += (warp - signals.warpYaw) * smoothingWeight(deltaSeconds, tuning.warpSmoothing);

        const float speed = math::length(velocity);
        signals.moveDirection = speed > k_minMoveSpeed ? toModel(velocity, yaw) * (1.0f / speed)
                                                       : math::Vec3{0.0f, 0.0f, 0.0f};

        const math::Vec3 leanTarget = tuning.isLeaning ? toModel(signals.acceleration, yaw) * tuning.leanPerAcceleration
                                                       : math::Vec3{0.0f, 0.0f, 0.0f};
        math::dampedSpring(signals.lean.x, signals.leanSpeed.x, leanTarget.x, tuning.leanFrequency, tuning.leanDamping,
            deltaSeconds);
        math::dampedSpring(signals.lean.z, signals.leanSpeed.z, leanTarget.z, tuning.leanFrequency, tuning.leanDamping,
            deltaSeconds);
    }

    float lowerBodyYaw(const LocomotionSignals& signals, float yaw)
    {
        return wrapAngle(signals.bodyYaw - yaw) + signals.warpYaw;
    }

    anim::LocomotionPoseInput locomotionPoseInput(const LocomotionSignals* signals, float yaw,
        const math::Transform& placement, bool isGrounded, float deltaSeconds)
    {
        anim::LocomotionPoseInput input{.placement = placement, .isGrounded = isGrounded, .deltaSeconds = deltaSeconds};

        if (signals == nullptr)
        {
            return input;
        }

        input.lowerBodyYaw = lowerBodyYaw(*signals, yaw);
        input.moveDirection = signals->moveDirection;
        input.strideScale = signals->strideScale;
        input.lean = signals->lean;
        input.footLift = signals->footLift;
        input.footLock = signals->footLock;
        return input;
    }

    float stridePlayback(const LocomotionTuning& tuning, float speed, float strideSpeed, float minRate, float maxRate,
        float& strideScale)
    {
        const float ratio = strideSpeed > 0.0f ? speed / strideSpeed : 1.0f;

        if (!tuning.isStrideWarped)
        {
            strideScale = 1.0f;
            return std::clamp(ratio, minRate, maxRate);
        }

        const float rate = std::clamp(std::pow(std::max(ratio, 0.01f), tuning.playbackShare), minRate, maxRate);
        strideScale = ratio / rate;
        return rate;
    }

    void disableLocomotionLayers(LocomotionTuning& tuning)
    {
        tuning.isInertialized = false;
        tuning.isStrideWarped = false;
        tuning.isOrientationWarped = false;
        tuning.isLeaning = false;
        tuning.isTurningInPlace = false;
        tuning.pose.footIk = false;
        tuning.isMotionMatched = false;
    }

#if LOCKSTEP_DEV_TOOLS
    void locomotionTuningDevSection(LocomotionTuning& tuning)
    {
        if (!ImGui::TreeNode("Locomotion layers (NPCs, partner, mirror)"))
        {
            return;
        }

        anim::LocomotionPoseSettings& pose = tuning.pose;
        ImGui::Checkbox("Inertialization", &tuning.isInertialized);
        ImGui::SameLine();
        ImGui::Checkbox("Foot IK", &pose.footIk);
        ImGui::SameLine();
        ImGui::Checkbox("Stride warp", &tuning.isStrideWarped);
        ImGui::Checkbox("Orientation warp", &tuning.isOrientationWarped);
        ImGui::SameLine();
        ImGui::Checkbox("Lean", &tuning.isLeaning);
        ImGui::SameLine();
        ImGui::Checkbox("Turn in place", &tuning.isTurningInPlace);
        ImGui::Checkbox("Motion matching (armed bodies with MM_ clips)", &tuning.isMotionMatched);

        if (ImGui::TreeNode("Motion matching"))
        {
            ImGui::SliderFloat("Search interval (s)", &tuning.matching.searchInterval, 0.02f, 0.5f);
            ImGui::SliderFloat("Min improvement", &tuning.matching.minImprovement, 0.0f, 3.0f);
            ImGui::SliderFloat("Same clip window (s)", &tuning.matching.sameClipWindow, 0.0f, 1.0f);
            ImGui::SliderFloat("Anticipation (s)", &tuning.anticipation, 0.0f, 1.0f);
            ImGui::SliderFloat("Turn anticipation (s)", &tuning.turnAnticipation, 0.0f, 1.0f);
            ImGui::SliderFloat("Trajectory half-life (s)", &tuning.trajectoryHalflife, 0.05f, 1.0f);
            ImGui::SliderFloat("Jump fade (s)", &tuning.matchFade, 0.0f, 0.6f);
            ImGui::SliderFloat("Max warp (rad)", &tuning.maxMatchWarp, 0.0f, 1.5f);
            ImGui::TreePop();
        }

        if (ImGui::TreeNode("Blending and warping"))
        {
            ImGui::SliderFloat("Half-life per fade second", &tuning.halflifePerFade, 0.05f, 1.0f);
            ImGui::SliderFloat("Play rate share", &tuning.playbackShare, 0.0f, 1.0f);
            ImGui::SliderFloat("Stride scale min", &pose.minStrideScale, 0.2f, 1.0f);
            ImGui::SliderFloat("Stride scale max", &pose.maxStrideScale, 1.0f, 2.5f);
            ImGui::SliderFloat("Warp smoothing (s)", &tuning.warpSmoothing, 0.0f, 0.5f);
            ImGui::SliderFloat("Max lower body yaw (rad)", &pose.maxLowerBodyYaw, 0.2f, 2.0f);
            ImGui::TreePop();
        }

        if (ImGui::TreeNode("Lean"))
        {
            ImGui::SliderFloat("Per m/s^2 (rad)", &tuning.leanPerAcceleration, 0.0f, 0.15f);
            ImGui::SliderFloat("Frequency (Hz)", &tuning.leanFrequency, 0.3f, 6.0f);
            ImGui::SliderFloat("Damping ratio", &tuning.leanDamping, 0.1f, 1.5f);
            ImGui::SliderFloat("Max lean (rad)", &pose.maxLean, 0.0f, 0.6f);
            ImGui::TreePop();
        }

        if (ImGui::TreeNode("Turn in place"))
        {
            ImGui::SliderFloat("Start angle (rad)", &tuning.turnStartAngle, 0.2f, 1.6f);
            ImGui::SliderFloat("Settle angle (rad)", &tuning.turnSettleAngle, 0.05f, 1.0f);
            ImGui::SliderFloat("Settle after (s)", &tuning.turnSettleSeconds, 0.1f, 2.0f);
            ImGui::SliderFloat("Clip share", &tuning.turnClipShare, 0.0f, 1.5f);
            ImGui::SliderFloat("Clip rate", &tuning.turnClipRate, 0.5f, 2.5f);
            ImGui::SliderFloat("Shuffle (s)", &tuning.shuffleSeconds, 0.2f, 1.2f);
            ImGui::SliderFloat("Step height (m)", &tuning.stepHeight, 0.0f, 0.2f);
            ImGui::TreePop();
        }

        if (ImGui::TreeNode("Foot IK"))
        {
            ImGui::SliderFloat("Max raise (m)", &pose.maxFootRaise, 0.0f, 0.8f);
            ImGui::SliderFloat("Max drop (m)", &pose.maxFootDrop, 0.0f, 0.8f);
            ImGui::SliderFloat("Max pelvis drop (m)", &pose.maxPelvisDrop, 0.0f, 0.8f);
            ImGui::SliderFloat("Max foot tilt (rad)", &pose.maxFootTilt, 0.0f, 1.0f);
            ImGui::SliderFloat("Planted height (m)", &pose.plantedHeight, 0.01f, 0.3f);
            ImGui::SliderFloat("Foot half-life (s)", &pose.footHalflife, 0.0f, 0.3f);
            ImGui::SliderFloat("Pelvis half-life (s)", &pose.pelvisHalflife, 0.0f, 0.3f);
            ImGui::SliderFloat("Fade (s)", &pose.fadeSeconds, 0.0f, 1.0f);
            ImGui::SliderFloat("Lock release (m)", &pose.maxLockDistance, 0.05f, 1.0f);
            ImGui::TreePop();
        }

        ImGui::TreePop();
    }
#endif
} //namespace game
