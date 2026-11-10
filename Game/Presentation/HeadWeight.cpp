#include "Presentation/HeadWeight.hpp"

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
        constexpr float k_maxVelocityChange{10.0f};
        constexpr float k_minSeconds{0.0001f};

        float follow(float current, float target, float rate, float seconds)
        {
            return current + (target - current) * (1.0f - std::exp(-rate * seconds));
        }

        math::Vec3 rightOf(float yaw)
        {
            return math::Vec3{std::cos(yaw), 0.0f, -std::sin(yaw)};
        }

        math::Vec3 forwardOf(float yaw)
        {
            return math::Vec3{-std::sin(yaw), 0.0f, -std::cos(yaw)};
        }

        void leanIntoVelocityChange(HeadWeightState& state, const HeadWeightTuning& tuning,
            const HeadWeightInput& input)
        {
            math::Vec3 change = input.planarVelocity - state.lastVelocity;
            const float length = math::length(change);

            if (length > k_maxVelocityChange)
            {
                change = change * (k_maxVelocityChange / length);
            }

            if (!input.isOnFeet)
            {
                return;
            }

            const float forward = math::dot(change, forwardOf(input.yaw));
            const float sideways = math::dot(change, rightOf(input.yaw));
            state.leanRate.x -= tuning.accelPitch * forward;
            state.leanRate.y += tuning.accelRoll * sideways;
        }

        void addEventImpulses(HeadWeightState& state, const HeadWeightTuning& tuning, const HeadWeightInput& input)
        {
            if (input.footfalls > 0 && input.isOnFeet)
            {
                const float strength = input.stepStrength * static_cast<float>(std::min(input.footfalls, 2u));
                state.plantRate.x -= tuning.plantDrop * strength;
                state.plantRate.y -= tuning.plantPitch * strength;
            }

            if (input.isTakeoff)
            {
                state.leanRate.x -= tuning.jumpPitch;
            }

            if (input.landSpeed >= tuning.landPitchMinSpeed)
            {
                state.leanRate.x -= tuning.landPitch * input.landSpeed;
            }
        }
    } //namespace

    HeadWeightPose updateHeadWeight(HeadWeightState& state, const HeadWeightTuning& tuning, const HeadWeightInput& input)
    {
        const float seconds = input.seconds;

        if (!state.hasLast)
        {
            state = HeadWeightState{.lastVelocity = input.planarVelocity, .lastYaw = input.yaw, .hasLast = true};
        }

        leanIntoVelocityChange(state, tuning, input);
        addEventImpulses(state, tuning, input);
        state.lastVelocity = input.planarVelocity;

        const float rawTurnRate = seconds > k_minSeconds
            ? std::remainder(input.yaw - state.lastYaw, 2.0f * math::k_pi) / seconds : 0.0f;
        state.lastYaw = input.yaw;
        state.turnRate = follow(state.turnRate, rawTurnRate, tuning.turnSmoothRate, seconds);

        stepSpring(state.lean, state.leanRate, tuning.leanStiffness, tuning.leanDamping, seconds);
        state.lean.x = math::clamp(state.lean.x, -tuning.leanLimit, tuning.leanLimit);
        state.lean.y = math::clamp(state.lean.y, -tuning.leanLimit, tuning.leanLimit);
        stepSpring(state.plant, state.plantRate, tuning.plantStiffness, tuning.plantDamping, seconds);

        const float aimScale = input.isAiming ? tuning.aimLagScale : 1.0f;
        const float speed = input.isOnFeet ? math::length(input.planarVelocity) : 0.0f;
        const float turnRoll = math::clamp(-tuning.turnRoll * speed * state.turnRate, -tuning.turnRollLimit,
            tuning.turnRollLimit);
        state.yawLag = math::clamp(-tuning.headLag * state.turnRate, -tuning.headLagLimit, tuning.headLagLimit)
            * aimScale;

        return HeadWeightPose{.pitch = state.lean.x + state.plant.y,
            .roll = state.lean.y + turnRoll * aimScale,
            .yawOffset = state.yawLag,
            .drop = state.plant.x};
    }

#if LOCKSTEP_DEV_TOOLS
    void headWeightDevSection(const HeadWeightState& state, HeadWeightTuning& tuning)
    {
        if (!ImGui::TreeNode("Body weight"))
        {
            return;
        }

        ImGui::Text("Lean %+.3f %+.3f  plant %+.3f m  turn %+.2f rad/s  lag %+.4f", state.lean.x, state.lean.y,
            state.plant.x, state.turnRate, state.yawLag);
        ImGui::SliderFloat("Accel pitch (rad/s per m/s)", &tuning.accelPitch, -0.3f, 0.3f);
        ImGui::SliderFloat("Accel roll (rad/s per m/s)", &tuning.accelRoll, -0.3f, 0.3f);
        ImGui::SliderFloat("Lean stiffness", &tuning.leanStiffness, 10.0f, 600.0f);
        ImGui::SliderFloat("Lean damping", &tuning.leanDamping, 1.0f, 60.0f);
        ImGui::SliderFloat("Lean limit (rad)", &tuning.leanLimit, 0.0f, 0.15f);
        ImGui::SliderFloat("Turn roll", &tuning.turnRoll, 0.0f, 0.02f, "%.4f");
        ImGui::SliderFloat("Turn roll limit (rad)", &tuning.turnRollLimit, 0.0f, 0.1f);
        ImGui::SliderFloat("Head lag (rad per rad/s)", &tuning.headLag, 0.0f, 0.01f, "%.4f");
        ImGui::SliderFloat("Head lag limit (rad)", &tuning.headLagLimit, 0.0f, 0.05f);
        ImGui::SliderFloat("Lag/turn roll x, aiming", &tuning.aimLagScale, 0.0f, 1.0f);
        ImGui::SliderFloat("Turn smoothing rate", &tuning.turnSmoothRate, 1.0f, 60.0f);
        ImGui::SliderFloat("Footfall drop (m/s)", &tuning.plantDrop, 0.0f, 0.5f);
        ImGui::SliderFloat("Footfall pitch (rad/s)", &tuning.plantPitch, 0.0f, 0.3f);
        ImGui::SliderFloat("Footfall stiffness", &tuning.plantStiffness, 50.0f, 1000.0f);
        ImGui::SliderFloat("Footfall damping", &tuning.plantDamping, 1.0f, 80.0f);
        ImGui::SliderFloat("Jump take-off pitch (rad/s)", &tuning.jumpPitch, 0.0f, 1.0f);
        ImGui::SliderFloat("Landing pitch (rad/s per m/s)", &tuning.landPitch, 0.0f, 0.3f);
        ImGui::SliderFloat("Landing pitch from (m/s)", &tuning.landPitchMinSpeed, 0.0f, 10.0f);
        ImGui::TreePop();
    }
#endif
} //namespace game
