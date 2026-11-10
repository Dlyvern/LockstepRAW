#include "Presentation/HeadMotion.hpp"

#include "Presentation/Springs.hpp"

#if LOCKSTEP_DEV_TOOLS
#include <imgui.h>
#endif

#include <algorithm>
#include <cmath>

namespace game
{
    namespace
    {
        constexpr float k_maxFrameSeconds{0.1f};
        constexpr float k_minSpeed{0.1f};
        constexpr float k_maxBobStrength{2.5f};

        float follow(float current, float target, float rate, float seconds)
        {
            return current + (target - current) * (1.0f - std::exp(-rate * seconds));
        }

        math::Vec3 rightOf(float yaw)
        {
            return math::Vec3{std::cos(yaw), 0.0f, -std::sin(yaw)};
        }

        void updateStepOffset(HeadMotionState& state, const HeadMotionInput& input, float seconds)
        {
            const HeadMotionTuning& tuning = state.tuning;
            const float rise = input.foot.y - state.lastFootY;
            state.lastFootY = input.foot.y;
            const bool isStep = input.isGrounded && state.wasGrounded && seconds > 0.0f
                && std::abs(rise) > tuning.stepMinSpeed * seconds;

            if (isStep || input.isVaulting)
            {
                state.stepOffset -= rise;
            }

            if (std::abs(state.stepOffset) > (input.isVaulting ? tuning.vaultMaxOffset : tuning.stepMaxOffset))
            {
                state.stepOffset = 0.0f;
            }

            state.stepOffset = follow(state.stepOffset, 0.0f,
                input.isVaulting ? tuning.vaultSmoothRate : tuning.stepSmoothRate, seconds);
        }

        uint32_t updateBob(HeadMotionState& state, const HeadMotionInput& input, float seconds)
        {
            const HeadMotionTuning& tuning = state.tuning;
            const bool isStepping = input.isGrounded && !input.isSliding;
            const float speed = isStepping ? math::length(input.planarVelocity) : 0.0f;
            const float runBlend = math::clamp((speed - input.walkSpeed)
                    / std::max(input.runSpeed - input.walkSpeed, k_minSpeed), 0.0f, 1.0f);
            const float strideBlend = math::clamp((speed - tuning.walkStrideSpeed)
                    / std::max(tuning.runStrideSpeed - tuning.walkStrideSpeed, k_minSpeed), 0.0f, 1.0f);
            const float stride = input.isCrouched
                ? tuning.crouchStride : tuning.walkStride + (tuning.runStride - tuning.walkStride) * strideBlend;
            const float depth = input.isCrouched ? tuning.crouchBob
                                                 : tuning.walkBob + (tuning.runBob - tuning.walkBob) * runBlend;
            const float weight = std::min(speed / std::max(input.walkSpeed, k_minSpeed), 1.0f)
                * (1.0f + (tuning.bulletTimeBob - 1.0f) * state.bulletTimeWeight);

            state.bobWeight = follow(state.bobWeight, weight, tuning.bobSmoothing, seconds);
            state.bobDepth = follow(state.bobDepth, depth, tuning.bobSmoothing, seconds);
            const float before = state.bobPhase;
            const float after = before + 2.0f * math::k_pi * speed / std::max(stride, k_minSpeed) * seconds;
            const float plantPhase = 0.5f * math::k_pi;
            const float footfalls = std::floor((after - plantPhase) / math::k_pi)
                - std::floor((before - plantPhase) / math::k_pi);
            const auto steps = static_cast<uint32_t>(std::max(footfalls, 0.0f));
            state.footfalls += steps;
            state.bobPhase = std::remainder(after, 2.0f * math::k_pi);

            return steps;
        }

        struct AirEvents
        {
            bool isTakeoff{false};
            float landSpeed{0.0f};
        };

        AirEvents updateLanding(HeadMotionState& state, const HeadMotionInput& input, float seconds)
        {
            const HeadMotionTuning& tuning = state.tuning;
            AirEvents events{};

            if (!input.isGrounded)
            {
                if (state.wasGrounded && input.verticalSpeed > 0.0f)
                {
                    events.isTakeoff = true;
                    state.dipRate -= tuning.jumpDip;
                }

                state.fallSpeed = std::max(state.fallSpeed, -input.verticalSpeed);
                state.airSeconds += seconds;
            }
            else if (!state.wasGrounded)
            {
                if (state.fallSpeed >= tuning.landMinSpeed && state.airSeconds >= tuning.landMinAirSeconds)
                {
                    state.dipRate -= tuning.landImpulse * state.fallSpeed;
                    events.landSpeed = state.fallSpeed;
                    state.landSpeed = state.fallSpeed;
                    ++state.landings;
                }

                state.fallSpeed = 0.0f;
                state.airSeconds = 0.0f;
            }

            state.wasGrounded = input.isGrounded;
            const float damping = tuning.landDamping
                * (1.0f + (tuning.bulletTimeLandDamping - 1.0f) * state.bulletTimeWeight);
            stepSpring(state.dip, state.dipRate, tuning.landStiffness, damping, seconds);
            state.dip = math::clamp(state.dip, -tuning.landMaxDip, tuning.landMaxDip);

            return events;
        }

        void updateRoll(HeadMotionState& state, const HeadMotionInput& input, float seconds)
        {
            const HeadMotionTuning& tuning = state.tuning;
            const float lateral = math::dot(input.planarVelocity, rightOf(input.yaw));
            const float strafe = math::clamp(lateral / std::max(input.strafeSpeed, k_minSpeed), -1.0f, 1.0f);
            const float target = input.isVaulting ? tuning.vaultRoll
                                                  : (input.isSliding ? tuning.slideRoll : strafe * tuning.strafeRoll);
            const float rate = tuning.rollRate * (1.0f + (tuning.bulletTimeRoll - 1.0f) * state.bulletTimeWeight);
            state.roll = follow(state.roll, target, rate, seconds);
        }

        void updateRecoil(HeadMotionState& state, const HeadMotionInput& input, float seconds)
        {
            const HeadMotionTuning& tuning = state.tuning;
            const auto shots = static_cast<uint8_t>(input.shotCount - state.shotCount);
            state.shotCount = input.shotCount;

            if (input.canKick && shots > 0)
            {
                const float aimScale = input.isAiming ? tuning.aimRecoilScale : 1.0f;
                const float crouchScale = input.isCrouched ? tuning.crouchRecoilScale : 1.0f;
                state.kickRate += tuning.recoilKick * aimScale * crouchScale * static_cast<float>(shots);
            }

            stepSpring(state.kick, state.kickRate, tuning.recoilStiffness, tuning.recoilDamping, seconds);
        }
    } //namespace

    HeadPose updateHeadMotion(HeadMotionState& state, const HeadMotionInput& input)
    {
        const float seconds = math::clamp(input.deltaSeconds, 0.0f, k_maxFrameSeconds);

        if (!state.isActive)
        {
            state = HeadMotionState{.tuning = state.tuning, .footfalls = state.footfalls, .landings = state.landings,
                .landSpeed = state.landSpeed};
            state.isActive = true;
            state.eyeHeight = input.eyeHeight;
            state.wasGrounded = input.isGrounded;
            state.shotCount = input.shotCount;
            state.lastFootY = input.foot.y;
        }

        const HeadMotionTuning& tuning = state.tuning;
        state.bulletTimeWeight = follow(state.bulletTimeWeight, input.isBulletTime ? 1.0f : 0.0f, tuning.bulletTimeRate,
            seconds);
        const float fovTarget = (input.isSprinting ? tuning.sprintFov : 0.0f)
            + tuning.bulletTimeFov * state.bulletTimeWeight;
        state.fovOffset = follow(state.fovOffset, fovTarget,
            fovTarget > state.fovOffset && input.isSprinting ? tuning.sprintFovRate : tuning.fovRate, seconds);
        state.eyeHeight = follow(state.eyeHeight, input.eyeHeight, tuning.eyeHeightRate, seconds);
        updateStepOffset(state, input, seconds);
        const uint32_t footfalls = updateBob(state, input, seconds);
        const AirEvents air = updateLanding(state, input, seconds);
        updateRoll(state, input, seconds);
        updateRecoil(state, input, seconds);

        const float walkDepth = std::max(tuning.walkBob, 0.001f);
        const float bobStrength = std::min(state.bobWeight * state.bobDepth / walkDepth, k_maxBobStrength);
        const HeadWeightPose weight = updateHeadWeight(state.weight, tuning.weight,
            HeadWeightInput{.yaw = input.yaw,
                .planarVelocity = input.planarVelocity,
                .isOnFeet = input.isGrounded && !input.isSliding,
                .isAiming = input.isAiming,
                .footfalls = footfalls,
                .stepStrength = bobStrength,
                .isTakeoff = air.isTakeoff,
                .isVaultStart = input.isVaulting && !state.wasVaulting,
                .isKickStart = input.kickSeconds >= 0.0f
                    && (state.lastKickSeconds < 0.0f || input.kickSeconds < state.lastKickSeconds),
                .isKickImpact = input.kickSeconds >= input.kickImpactSeconds
                    && (state.lastKickSeconds < input.kickImpactSeconds || input.kickSeconds < state.lastKickSeconds),
                .landSpeed = air.landSpeed,
                .seconds = seconds});
        state.wasVaulting = input.isVaulting;
        state.lastKickSeconds = input.kickSeconds;

        const float bob = state.bobDepth * state.bobWeight;
        const float vertical = -bob * (1.0f - std::abs(std::cos(state.bobPhase)));
        const float side = bob * tuning.bobSide * std::cos(state.bobPhase);
        const math::Vec3 eye = input.foot
            + math::Vec3{0.0f, state.eyeHeight + state.stepOffset + vertical + state.dip + weight.drop, 0.0f}
            + rightOf(input.yaw) * side + input.leanOffset
            + math::Vec3{-std::sin(input.yaw), 0.0f, -std::cos(input.yaw)} * weight.forward;

        return HeadPose{.eye = eye,
            .pitchOffset = state.kick + weight.pitch,
            .yawOffset = weight.yawOffset,
            .roll = state.roll + weight.roll + input.lean * tuning.leanRoll,
            .bobPhase = state.bobPhase,
            .bobStrength = bobStrength,
            .footfalls = state.footfalls,
            .landings = state.landings,
            .landSpeed = state.landSpeed,
            .fovOffset = state.fovOffset};
    }

    void resetHeadMotion(HeadMotionState& state)
    {
        state.isActive = false;
    }

    math::Vec3 rolledUp(float yaw, float pitch, float roll)
    {
        const math::Quat camera = math::angleAxis(yaw, math::Vec3{0.0f, 1.0f, 0.0f})
            * math::angleAxis(pitch, math::Vec3{1.0f, 0.0f, 0.0f});

        return camera * math::Vec3{std::sin(roll), std::cos(roll), 0.0f};
    }

#if LOCKSTEP_DEV_TOOLS
    void headMotionDevSection(HeadMotionState& state)
    {
        if (!ImGui::TreeNode("Head motion"))
        {
            return;
        }

        HeadMotionTuning& tuning = state.tuning;
        ImGui::Text("Eye %.2f m  step %.3f m  bob %.2f  dip %.3f m  roll %.3f  kick %.3f  fov %+.3f", state.eyeHeight,
            state.stepOffset, state.bobWeight, state.dip, state.roll, state.kick, state.fovOffset);
        ImGui::SliderFloat("Eye height rate", &tuning.eyeHeightRate, 1.0f, 40.0f);
        ImGui::SliderFloat("Step smoothing rate", &tuning.stepSmoothRate, 5.0f, 80.0f);
        ImGui::SliderFloat("Step min speed (m/s)", &tuning.stepMinSpeed, 1.0f, 20.0f);
        ImGui::SliderFloat("Step max offset (m)", &tuning.stepMaxOffset, 0.1f, 1.0f);
        ImGui::SliderFloat("Walk stride (m)", &tuning.walkStride, 0.5f, 4.0f);
        ImGui::SliderFloat("Run stride (m)", &tuning.runStride, 0.5f, 4.0f);
        ImGui::SliderFloat("Crouch stride (m)", &tuning.crouchStride, 0.5f, 4.0f);
        ImGui::SliderFloat("Walk stride speed (m/s)", &tuning.walkStrideSpeed, 0.5f, 8.0f);
        ImGui::SliderFloat("Run stride speed (m/s)", &tuning.runStrideSpeed, 0.5f, 10.0f);
        ImGui::SliderFloat("Walk bob (m)", &tuning.walkBob, 0.0f, 0.08f);
        ImGui::SliderFloat("Run bob (m)", &tuning.runBob, 0.0f, 0.08f);
        ImGui::SliderFloat("Crouch bob (m)", &tuning.crouchBob, 0.0f, 0.08f);
        ImGui::SliderFloat("Bob sideways", &tuning.bobSide, 0.0f, 2.0f);
        ImGui::SliderFloat("Land impulse", &tuning.landImpulse, 0.0f, 1.0f);
        ImGui::SliderFloat("Land min fall (m/s)", &tuning.landMinSpeed, 0.0f, 10.0f);
        ImGui::SliderFloat("Land min air time (s)", &tuning.landMinAirSeconds, 0.0f, 0.5f);
        ImGui::SliderFloat("Land max dip (m)", &tuning.landMaxDip, 0.0f, 0.3f);
        ImGui::SliderFloat("Land stiffness", &tuning.landStiffness, 10.0f, 400.0f);
        ImGui::SliderFloat("Land damping", &tuning.landDamping, 1.0f, 40.0f);
        ImGui::SliderFloat("Jump take-off dip (m/s)", &tuning.jumpDip, 0.0f, 2.0f);
        ImGui::SliderFloat("Strafe roll (rad)", &tuning.strafeRoll, 0.0f, 0.08f);
        ImGui::SliderFloat("Slide roll (rad)", &tuning.slideRoll, -0.2f, 0.2f);
        ImGui::SliderFloat("Vault roll (rad)", &tuning.vaultRoll, -0.3f, 0.3f);
        ImGui::SliderFloat("Vault camera smoothing rate", &tuning.vaultSmoothRate, 2.0f, 60.0f);
        ImGui::SliderFloat("Vault max smoothed (m)", &tuning.vaultMaxOffset, 0.2f, 3.0f);
        ImGui::SliderFloat("Lean roll (rad)", &tuning.leanRoll, 0.0f, 0.4f);
        ImGui::SliderFloat("Sprint FOV (rad)", &tuning.sprintFov, 0.0f, 0.2f);
        ImGui::SliderFloat("Bullet-time FOV (rad)", &tuning.bulletTimeFov, -0.2f, 0.1f);
        ImGui::SliderFloat("Sprint FOV in rate", &tuning.sprintFovRate, 1.0f, 30.0f);
        ImGui::SliderFloat("FOV rate", &tuning.fovRate, 1.0f, 30.0f);
        ImGui::SliderFloat("Bullet-time ease rate", &tuning.bulletTimeRate, 0.5f, 20.0f);
        ImGui::SliderFloat("Bullet-time bob x", &tuning.bulletTimeBob, 0.0f, 1.5f);
        ImGui::SliderFloat("Bullet-time roll rate x", &tuning.bulletTimeRoll, 0.1f, 1.5f);
        ImGui::SliderFloat("Bullet-time landing damping x", &tuning.bulletTimeLandDamping, 0.5f, 3.0f);
        ImGui::SliderFloat("Roll rate", &tuning.rollRate, 1.0f, 30.0f);
        ImGui::SliderFloat("Camera kick (rad/s)", &tuning.recoilKick, 0.0f, 2.0f);
        ImGui::SliderFloat("Camera kick stiffness", &tuning.recoilStiffness, 20.0f, 600.0f);
        ImGui::SliderFloat("Camera kick damping", &tuning.recoilDamping, 1.0f, 60.0f);
        ImGui::SliderFloat("Kick scale, aiming", &tuning.aimRecoilScale, 0.0f, 1.0f);
        ImGui::SliderFloat("Kick scale, crouched", &tuning.crouchRecoilScale, 0.0f, 1.0f);
        headWeightDevSection(state.weight, tuning.weight);
        ImGui::TreePop();
    }
#endif
} //namespace game
