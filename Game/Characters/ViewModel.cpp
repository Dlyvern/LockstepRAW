#include "Characters/ViewModel.hpp"

#include <Lockstep/Core/Transform.hpp>
#include <Lockstep/Render/Renderer.hpp>
#include <Lockstep/Render/Views.hpp>

#if LOCKSTEP_DEV_TOOLS
#include <imgui.h>
#endif

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <string>
#include <utility>

namespace game
{
    namespace
    {
        namespace anim = lockstep::anim;
        namespace render = lockstep::render;

        constexpr ViewModelPlacement k_fpSightsPlacement{
            .rotation = math::Quat{0.0001f, -0.9289f, 0.1983f, 0.3128f},
            .origin = {0.2685f, -1.1199f, 0.5497f}};
        constexpr ViewModelPlacement k_fpReadyPlacement{
            .rotation = math::Quat{0.0553f, -0.9045f, 0.2084f, 0.3679f},
            .origin = {0.6485f, -1.1450f, 0.2895f}};
        constexpr ViewModelPlacement k_fpLoweredPlacement{
            .rotation = math::Quat{-0.0582f, -0.9964f, -0.0594f, -0.0138f},
            .origin = {0.0996f, -1.3563f, -0.3354f}};
        constexpr math::Vec3 k_weaponAnchor{-0.13f, 1.31f, 0.37f};
        constexpr float k_maxRecoil{0.15f};
        constexpr float k_maxPush{0.04f};

        constexpr math::Vec3 k_viewRight{1.0f, 0.0f, 0.0f};
        constexpr math::Vec3 k_viewUp{0.0f, 1.0f, 0.0f};
        constexpr math::Vec3 k_viewBack{0.0f, 0.0f, 1.0f};
        constexpr float k_maxSpringStep{1.0f / 240.0f};
        constexpr float k_maxBobWeight{2.5f};
        constexpr float k_bobSmoothing{8.0f};
        constexpr float k_bobVertical{0.66f};
        constexpr float k_maxFrameSeconds{0.1f};
        constexpr std::string_view k_headJoint{"Head"};

        bool endsWith(std::string_view text, std::string_view suffix)
        {
            return text.size() >= suffix.size() && text.substr(text.size() - suffix.size()) == suffix;
        }

        void logAimHead(const FirstPersonModel& model)
        {
            const anim::SkinnedModel& skinned = model.character.skinned();
            anim::SkinnedInstance probe{};
            probe.animator().play(model.aimClip, 0.0f);
            probe.update(skinned, 0.0f);

            for (uint32_t joint = 0; joint < skinned.skeleton.joints.size(); ++joint)
            {
                if (!endsWith(skinned.skeleton.joints[joint].name, k_headJoint))
                {
                    continue;
                }

                const math::Vec4 head = probe.jointMatrices()[joint].getColumn(3);
                std::clog << std::fixed << std::setprecision(3) << "Game: viewmodel '" << skinned.name
                          << "' aim-pose head at (" << head.x << ", " << head.y << ", " << head.z << ")\n";
                return;
            }
        }

        float wrapAngle(float angle)
        {
            return std::remainder(angle, 2.0f * math::k_pi);
        }

        float smoothstep(float value)
        {
            const float clamped = std::clamp(value, 0.0f, 1.0f);
            return clamped * clamped * (3.0f - 2.0f * clamped);
        }

        template<typename T>
        void stepSpring(T& position, T& rate, float stiffness, float damping, float seconds)
        {
            const int steps = std::max(1, static_cast<int>(std::ceil(seconds / k_maxSpringStep)));
            const float step = seconds / static_cast<float>(steps);

            for (int index = 0; index < steps; ++index)
            {
                rate = rate + (position * -stiffness - rate * damping) * step;
                position = position + rate * step;
            }
        }

        void restart(ViewModelState& state, const FirstPersonModel& model, const ViewModelInput& input)
        {
            state.aimBlend = input.isAiming ? 1.0f : 0.0f;
            state.raiseBlend = input.isLowered ? 0.0f : 1.0f;
            state.sway = math::Vec2{0.0f, 0.0f};
            state.swayRate = math::Vec2{0.0f, 0.0f};
            state.bobWeight = 0.0f;
            state.recoil = 0.0f;
            state.recoilRate = 0.0f;
            state.shotCount = input.shotCount;
            state.lastYaw = input.yaw;
            state.lastPitch = input.pitch;
            state.instance.animator().play(input.isLowered ? model.lowClip : model.aimClip, 0.0f);
        }

        void updateMotion(ViewModelState& state, const ViewModelInput& input, float seconds)
        {
            const ViewModelTuning& tuning = state.tuning;
            const float aimStep = tuning.aimSeconds > 0.0f ? seconds / tuning.aimSeconds : 1.0f;
            state.aimBlend = std::clamp(state.aimBlend + (input.isAiming ? aimStep : -aimStep), 0.0f, 1.0f);
            const float raiseStep = tuning.raiseSeconds > 0.0f ? seconds / tuning.raiseSeconds : 1.0f;
            state.raiseBlend = std::clamp(state.raiseBlend + (input.isLowered ? -raiseStep : raiseStep), 0.0f, 1.0f);

            const math::Vec2 look{wrapAngle(input.yaw - state.lastYaw), input.pitch - state.lastPitch};
            state.lastYaw = input.yaw;
            state.lastPitch = input.pitch;
            state.sway = state.sway - look * tuning.swayLag;
            state.sway.x = std::clamp(state.sway.x, -tuning.swayLimit, tuning.swayLimit);
            state.sway.y = std::clamp(state.sway.y, -tuning.swayLimit, tuning.swayLimit);
            stepSpring(state.sway, state.swayRate, tuning.swayStiffness, tuning.swayDamping, seconds);

            const float walkSpeed = std::max(input.walkSpeed, 0.1f);
            const float bobTarget = std::min(input.speed / walkSpeed, k_maxBobWeight);
            state.bobWeight += (bobTarget - state.bobWeight) * (1.0f - std::exp(-k_bobSmoothing * seconds));
            state.bobPhase = wrapAngle(state.bobPhase + 2.0f * math::k_pi * input.speed / std::max(tuning.bobStride, 0.1f) * seconds);

            const auto shots = static_cast<uint8_t>(input.shotCount - state.shotCount);
            state.shotCount = input.shotCount;

            if (input.canFire && !input.isLowered)
            {
                state.recoilRate += tuning.recoilKick * static_cast<float>(shots);
            }

            stepSpring(state.recoil, state.recoilRate, tuning.recoilStiffness, tuning.recoilDamping, seconds);
            state.recoil = std::clamp(state.recoil, -k_maxRecoil, k_maxRecoil);
        }

        struct AnchoredPose
        {
            math::Quat rotation{};
            math::Vec3 anchor{0.0f, 0.0f, 0.0f};
        };

        AnchoredPose anchored(const ViewModelPlacement& placement)
        {
            return AnchoredPose{.rotation = placement.rotation,
                .anchor = placement.origin + placement.rotation * k_weaponAnchor};
        }

        AnchoredPose blendPoses(const AnchoredPose& from, const AnchoredPose& to, float amount)
        {
            return AnchoredPose{.rotation = math::slerp(from.rotation, to.rotation, amount),
                .anchor = math::lerp(from.anchor, to.anchor, amount)};
        }

        math::Vec3 clampLength(const math::Vec3& vector, float maxLength)
        {
            const float length = math::length(vector);

            return length > maxLength ? vector * (maxLength / length) : vector;
        }

        math::Transform viewModelTransform(const ViewModelState& state, const ViewModelInput& input)
        {
            const ViewModelTuning& tuning = state.tuning;
            const float aimed = smoothstep(state.aimBlend);
            const float motion = 1.0f + (tuning.aimDamping - 1.0f) * aimed;

            const float raised = smoothstep(state.raiseBlend);
            const AnchoredPose up = blendPoses(anchored(k_fpReadyPlacement), anchored(k_fpSightsPlacement), aimed);
            const AnchoredPose pose = blendPoses(anchored(k_fpLoweredPlacement), up, raised);
            const math::Vec3 anchor = pose.anchor;
            math::Quat rotation = pose.rotation;
            math::Vec3 origin = anchor - rotation * k_weaponAnchor;

            const float bob = tuning.bobAmount * state.bobWeight * motion;
            const math::Quat kick = math::angleAxis(state.recoil * motion, k_viewRight)
                * math::angleAxis(std::sin(state.bobPhase) * tuning.bobRoll * state.bobWeight * motion, k_viewBack);
            origin = anchor + kick * (origin - anchor);
            rotation = kick * rotation;

            const float dip = std::sin(math::k_pi * aimed) * tuning.aimDip;
            const math::Vec3 push{std::sin(state.bobPhase) * bob,
                -(1.0f - std::cos(2.0f * state.bobPhase)) * 0.5f * bob * k_bobVertical - dip,
                state.recoil * motion * tuning.recoilBack + dip * 0.5f};
            origin = origin + math::lerp(tuning.readyOffset, tuning.aimOffset, aimed) + clampLength(push, k_maxPush);

            const math::Quat sway = math::angleAxis(state.sway.x * motion, k_viewUp)
                * math::angleAxis(state.sway.y * motion, k_viewRight);
            origin = sway * origin;
            rotation = sway * rotation;

            const math::Quat camera = math::angleAxis(input.yaw, k_viewUp) * math::angleAxis(input.pitch, k_viewRight);

            return math::Transform{.position = input.eye + camera * origin, .rotation = camera * rotation};
        }
    } //namespace

    lockstep::Result<FirstPersonModel> createFirstPersonModel(CharacterModel&& character, std::string_view aimClip,
        std::string_view lowClip)
    {
        const anim::SkinnedModel& skinned = character.skinned();
        const std::optional<uint32_t> aim = anim::findClip(skinned.clips, aimClip);
        const std::optional<uint32_t> low = anim::findClip(skinned.clips, lowClip);

        if (!aim || !low)
        {
            return lockstep::Error{lockstep::ErrorCode::InvalidArgument, "viewmodel '" + skinned.name + "' has no clip '"
                + std::string(aim ? lowClip : aimClip) + "'"};
        }

        FirstPersonModel model{.character = std::move(character), .aimClip = *aim, .lowClip = *low};
        logAimHead(model);

        return model;
    }

    void drawViewModel(render::Renderer& renderer, const FirstPersonModel& model, ViewModelState& state,
        const ViewModelInput& input)
    {
        const float seconds = std::clamp(input.deltaSeconds, 0.0f, k_maxFrameSeconds);

        if (!state.isDrawn)
        {
            restart(state, model, input);
        }

        state.isDrawn = true;
        updateMotion(state, input, seconds);

        const anim::SkinnedModel& skinned = model.character.skinned();
        state.instance.animator().play(input.isLowered ? model.lowClip : model.aimClip, state.tuning.raiseSeconds);
        state.instance.update(skinned, seconds);
        state.instance.submit(renderer, skinned, viewModelTransform(state, input), render::k_viewModelLayer);
    }

    float viewModelZoom(const ViewModelState& state)
    {
        return state.isDrawn ? smoothstep(state.aimBlend) : 0.0f;
    }

    void hideViewModel(ViewModelState& state)
    {
        state.isDrawn = false;
    }

#if LOCKSTEP_DEV_TOOLS
    void viewModelDevSection(ViewModelState& state)
    {
        if (!ImGui::TreeNode("Viewmodel"))
        {
            return;
        }

        ViewModelTuning& tuning = state.tuning;
        ImGui::Text("Aim %.2f  raise %.2f  sway %.3f %.3f  bob %.2f  recoil %.3f", state.aimBlend, state.raiseBlend,
            state.sway.x, state.sway.y, state.bobWeight, state.recoil);
        ImGui::SliderFloat("Aim time (s)", &tuning.aimSeconds, 0.0f, 0.6f);
        ImGui::SliderFloat("Raise time (s)", &tuning.raiseSeconds, 0.0f, 1.0f);
        ImGui::SliderFloat("Aim dip (m)", &tuning.aimDip, 0.0f, 0.05f);
        ImGui::SliderFloat("Sway lag", &tuning.swayLag, 0.0f, 0.5f);
        ImGui::SliderFloat("Sway limit (rad)", &tuning.swayLimit, 0.0f, 0.2f);
        ImGui::SliderFloat("Sway stiffness", &tuning.swayStiffness, 10.0f, 400.0f);
        ImGui::SliderFloat("Sway damping", &tuning.swayDamping, 1.0f, 40.0f);
        ImGui::SliderFloat("Bob (m)", &tuning.bobAmount, 0.0f, 0.03f);
        ImGui::SliderFloat("Bob roll (rad)", &tuning.bobRoll, 0.0f, 0.05f);
        ImGui::SliderFloat("Bob stride (m)", &tuning.bobStride, 0.5f, 3.0f);
        ImGui::SliderFloat("Recoil kick (rad/s)", &tuning.recoilKick, 0.0f, 4.0f);
        ImGui::SliderFloat("Recoil back (m/rad)", &tuning.recoilBack, 0.0f, 4.0f);
        ImGui::SliderFloat("Recoil stiffness", &tuning.recoilStiffness, 50.0f, 800.0f);
        ImGui::SliderFloat("Recoil damping", &tuning.recoilDamping, 1.0f, 60.0f);
        ImGui::SliderFloat("Aim motion scale", &tuning.aimDamping, 0.0f, 1.0f);
        ImGui::DragFloat3("Ready offset (m)", &tuning.readyOffset.x, 0.002f, -0.1f, 0.1f);
        ImGui::DragFloat3("Aim offset (m)", &tuning.aimOffset.x, 0.002f, -0.05f, 0.05f);
        ImGui::TreePop();
    }
#endif
} //namespace game
