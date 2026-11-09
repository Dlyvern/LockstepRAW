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

        void updateBob(HeadMotionState& state, const HeadMotionInput& input, float seconds)
        {
            const HeadMotionTuning& tuning = state.tuning;
            const bool isStepping = input.isGrounded && !input.isSliding;
            const float speed = isStepping ? math::length(input.planarVelocity) : 0.0f;
            const float runBlend = math::clamp((speed - input.walkSpeed)
                    / std::max(input.runSpeed - input.walkSpeed, k_minSpeed), 0.0f, 1.0f);
            const float stride = input.isCrouched ? tuning.crouchStride
                                                  : tuning.walkStride + (tuning.runStride - tuning.walkStride) * runBlend;
            const float depth = input.isCrouched ? tuning.crouchBob
                                                 : tuning.walkBob + (tuning.runBob - tuning.walkBob) * runBlend;
            const float weight = std::min(speed / std::max(input.walkSpeed, k_minSpeed), 1.0f);

            state.bobWeight = follow(state.bobWeight, weight, tuning.bobSmoothing, seconds);
            state.bobDepth = follow(state.bobDepth, depth, tuning.bobSmoothing, seconds);
            state.bobPhase = std::remainder(
                state.bobPhase + 2.0f * math::k_pi * speed / std::max(stride, k_minSpeed) * seconds, 2.0f * math::k_pi);
        }

        void updateLanding(HeadMotionState& state, const HeadMotionInput& input, float seconds)
        {
            const HeadMotionTuning& tuning = state.tuning;

            if (!input.isGrounded)
            {
                state.fallSpeed = std::max(state.fallSpeed, -input.verticalSpeed);
            }
            else if (!state.wasGrounded)
            {
                if (state.fallSpeed >= tuning.landMinSpeed)
                {
                    state.dipRate -= tuning.landImpulse * state.fallSpeed;
                }

                state.fallSpeed = 0.0f;
            }

            state.wasGrounded = input.isGrounded;
            stepSpring(state.dip, state.dipRate, tuning.landStiffness, tuning.landDamping, seconds);
            state.dip = math::clamp(state.dip, -tuning.landMaxDip, tuning.landMaxDip);
        }

        void updateRoll(HeadMotionState& state, const HeadMotionInput& input, float seconds)
        {
            const HeadMotionTuning& tuning = state.tuning;
            const float lateral = math::dot(input.planarVelocity, rightOf(input.yaw));
            const float strafe = math::clamp(lateral / std::max(input.strafeSpeed, k_minSpeed), -1.0f, 1.0f);
            const float target = input.isSliding ? tuning.slideRoll : strafe * tuning.strafeRoll;
            state.roll = follow(state.roll, target, tuning.rollRate, seconds);
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
            const HeadMotionTuning tuning = state.tuning;
            state = HeadMotionState{.tuning = tuning};
            state.isActive = true;
            state.eyeHeight = input.eyeHeight;
            state.wasGrounded = input.isGrounded;
            state.shotCount = input.shotCount;
        }

        state.eyeHeight = follow(state.eyeHeight, input.eyeHeight, state.tuning.eyeHeightRate, seconds);
        updateBob(state, input, seconds);
        updateLanding(state, input, seconds);
        updateRoll(state, input, seconds);
        updateRecoil(state, input, seconds);

        const float bob = state.bobDepth * state.bobWeight;
        const float vertical = -bob * 0.5f * (1.0f - std::cos(2.0f * state.bobPhase));
        const float side = bob * state.tuning.bobSide * std::sin(state.bobPhase);
        const math::Vec3 eye = input.foot + math::Vec3{0.0f, state.eyeHeight + vertical + state.dip, 0.0f}
            + rightOf(input.yaw) * side;
        const float walkDepth = std::max(state.tuning.walkBob, 0.001f);

        return HeadPose{.eye = eye,
            .pitchOffset = state.kick,
            .roll = state.roll,
            .bobPhase = state.bobPhase,
            .bobStrength = std::min(state.bobWeight * state.bobDepth / walkDepth, k_maxBobStrength)};
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
        ImGui::Text("Eye %.2f m  bob %.2f  dip %.3f m  roll %.3f  kick %.3f", state.eyeHeight, state.bobWeight,
            state.dip, state.roll, state.kick);
        ImGui::SliderFloat("Eye height rate", &tuning.eyeHeightRate, 1.0f, 40.0f);
        ImGui::SliderFloat("Walk stride (m)", &tuning.walkStride, 0.5f, 4.0f);
        ImGui::SliderFloat("Run stride (m)", &tuning.runStride, 0.5f, 4.0f);
        ImGui::SliderFloat("Crouch stride (m)", &tuning.crouchStride, 0.5f, 4.0f);
        ImGui::SliderFloat("Walk bob (m)", &tuning.walkBob, 0.0f, 0.08f);
        ImGui::SliderFloat("Run bob (m)", &tuning.runBob, 0.0f, 0.08f);
        ImGui::SliderFloat("Crouch bob (m)", &tuning.crouchBob, 0.0f, 0.08f);
        ImGui::SliderFloat("Bob sideways", &tuning.bobSide, 0.0f, 2.0f);
        ImGui::SliderFloat("Land impulse", &tuning.landImpulse, 0.0f, 1.0f);
        ImGui::SliderFloat("Land min fall (m/s)", &tuning.landMinSpeed, 0.0f, 10.0f);
        ImGui::SliderFloat("Land max dip (m)", &tuning.landMaxDip, 0.0f, 0.3f);
        ImGui::SliderFloat("Land stiffness", &tuning.landStiffness, 10.0f, 400.0f);
        ImGui::SliderFloat("Land damping", &tuning.landDamping, 1.0f, 40.0f);
        ImGui::SliderFloat("Strafe roll (rad)", &tuning.strafeRoll, 0.0f, 0.08f);
        ImGui::SliderFloat("Slide roll (rad)", &tuning.slideRoll, -0.2f, 0.2f);
        ImGui::SliderFloat("Roll rate", &tuning.rollRate, 1.0f, 30.0f);
        ImGui::SliderFloat("Camera kick (rad/s)", &tuning.recoilKick, 0.0f, 2.0f);
        ImGui::SliderFloat("Camera kick stiffness", &tuning.recoilStiffness, 20.0f, 600.0f);
        ImGui::SliderFloat("Camera kick damping", &tuning.recoilDamping, 1.0f, 60.0f);
        ImGui::SliderFloat("Kick scale, aiming", &tuning.aimRecoilScale, 0.0f, 1.0f);
        ImGui::SliderFloat("Kick scale, crouched", &tuning.crouchRecoilScale, 0.0f, 1.0f);
        ImGui::TreePop();
    }
#endif
} //namespace game
