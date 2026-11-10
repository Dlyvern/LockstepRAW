#include "Characters/ViewModel.hpp"

#include "Presentation/Springs.hpp"

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
        constexpr math::Vec3 k_weaponAnchor{-0.13f, 1.31f, 0.37f};
        constexpr float k_maxRecoil{0.15f};
        constexpr float k_maxPush{0.04f};
        constexpr float k_maxVelocityChange{10.0f};

        constexpr math::Vec3 k_viewRight{1.0f, 0.0f, 0.0f};
        constexpr math::Vec3 k_viewUp{0.0f, 1.0f, 0.0f};
        constexpr math::Vec3 k_viewBack{0.0f, 0.0f, 1.0f};
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

        math::Vec3 clampLength(const math::Vec3& vector, float maxLength)
        {
            const float length = math::length(vector);

            return length > maxLength ? vector * (maxLength / length) : vector;
        }

        template<typename T>
        void followSpring(T& value, T& rate, const T& target, float stiffness, float damping, float seconds)
        {
            T displacement = value - target;
            stepSpring(displacement, rate, stiffness, damping, seconds);
            value = target + displacement;
        }

        math::Vec2 worldVelocity(const ViewModelInput& input)
        {
            const float cosine = std::cos(input.yaw);
            const float sine = std::sin(input.yaw);

            return math::Vec2{input.velocity.x * cosine - input.velocity.y * sine,
                -input.velocity.x * sine - input.velocity.y * cosine};
        }

        math::Vec2 viewVelocityChange(ViewModelState& state, const ViewModelInput& input)
        {
            const math::Vec2 world = worldVelocity(input);
            math::Vec2 change = world - state.lastVelocity;
            state.lastVelocity = world;
            const float length = math::length(change);

            if (length > k_maxVelocityChange)
            {
                change = change * (k_maxVelocityChange / length);
            }

            const float cosine = std::cos(input.yaw);
            const float sine = std::sin(input.yaw);

            return math::Vec2{change.x * cosine - change.y * sine, -change.x * sine - change.y * cosine};
        }

        void updateAirReaction(ViewModelState& state, const ViewModelInput& input, float seconds)
        {
            const ViewModelTuning& tuning = state.tuning;

            if (!input.isGrounded)
            {
                if (state.wasGrounded && input.verticalSpeed > 0.0f)
                {
                    state.airRate = state.airRate - math::Vec2{tuning.jumpDrop, tuning.jumpPitch};
                }

                state.fallSpeed = std::max(state.fallSpeed, -input.verticalSpeed);
            }
            else if (!state.wasGrounded)
            {
                if (state.fallSpeed >= tuning.landMinSpeed)
                {
                    state.airRate = state.airRate - math::Vec2{tuning.landDrop, tuning.landPitch} * state.fallSpeed;
                }

                state.fallSpeed = 0.0f;
            }

            state.wasGrounded = input.isGrounded;
            stepSpring(state.air, state.airRate, tuning.airStiffness, tuning.airDamping, seconds);
            state.air.x = std::clamp(state.air.x, -tuning.airLimit, tuning.airLimit);
            state.air.y = std::clamp(state.air.y, -2.0f * tuning.airLimit, 2.0f * tuning.airLimit);
        }

        void updateMovementReaction(ViewModelState& state, const ViewModelInput& input, float seconds)
        {
            const ViewModelTuning& tuning = state.tuning;
            const math::Vec2 change = viewVelocityChange(state, input);
            state.moveRate = state.moveRate + math::Vec3{-change.x, -std::abs(change.y) * tuning.moveDip, change.y}
                * tuning.moveKick;
            state.moveAngleRate = state.moveAngleRate
                + math::Vec2{-std::abs(change.y) * tuning.movePitch, change.x * tuning.moveYaw};
            stepSpring(state.moveAngle, state.moveAngleRate, tuning.moveStiffness, tuning.moveDamping, seconds);
            state.moveAngle.x = std::clamp(state.moveAngle.x, -tuning.moveAngleLimit, tuning.moveAngleLimit);
            state.moveAngle.y = std::clamp(state.moveAngle.y, -tuning.moveAngleLimit, tuning.moveAngleLimit);

            const float strafe = math::clamp(input.velocity.x / std::max(input.strafeSpeed, 0.1f), -1.5f, 1.5f);
            const math::Vec3 strafeTarget{-strafe * tuning.strafeShift, 0.0f, 0.0f};
            followSpring(state.moveOffset, state.moveRate, strafeTarget, tuning.moveStiffness, tuning.moveDamping,
                seconds);
            state.moveOffset = clampLength(state.moveOffset, tuning.moveLimit);
            followSpring(state.tilt, state.tiltRate, strafe * tuning.strafeTilt, tuning.moveStiffness,
                tuning.moveDamping, seconds);
        }

        void restart(ViewModelState& state, const FirstPersonModel& model, const ViewModelInput& input)
        {
            state.aimBlend = input.isAiming ? 1.0f : 0.0f;
            state.raiseBlend = input.isLowered ? 0.0f : 1.0f;
            state.sprintBlend = input.isSprinting ? 1.0f : 0.0f;
            state.sway = math::Vec2{0.0f, 0.0f};
            state.swayRate = math::Vec2{0.0f, 0.0f};
            state.recoil = 0.0f;
            state.recoilRate = 0.0f;
            state.tilt = 0.0f;
            state.tiltRate = 0.0f;
            state.moveOffset = math::Vec3{0.0f, 0.0f, 0.0f};
            state.moveRate = math::Vec3{0.0f, 0.0f, 0.0f};
            state.moveAngle = math::Vec2{0.0f, 0.0f};
            state.moveAngleRate = math::Vec2{0.0f, 0.0f};
            state.air = math::Vec2{0.0f, 0.0f};
            state.airRate = math::Vec2{0.0f, 0.0f};
            state.wasGrounded = input.isGrounded;
            state.fallSpeed = 0.0f;
            state.lastVelocity = worldVelocity(input);
            state.shotCount = input.shotCount;
            state.lastYaw = input.yaw;
            state.lastPitch = input.pitch;
            state.instance.animator().play(model.aimClip, 0.0f);
        }

        void updateMotion(ViewModelState& state, const ViewModelInput& input, float seconds)
        {
            const ViewModelTuning& tuning = state.tuning;
            const float aimStep = tuning.aimSeconds > 0.0f ? seconds / tuning.aimSeconds : 1.0f;
            state.aimBlend = std::clamp(state.aimBlend + (input.isAiming ? aimStep : -aimStep), 0.0f, 1.0f);
            const float raiseStep = tuning.raiseSeconds > 0.0f ? seconds / tuning.raiseSeconds : 1.0f;
            state.raiseBlend = std::clamp(state.raiseBlend + (input.isLowered ? -raiseStep : raiseStep), 0.0f, 1.0f);
            const float sprintStep = tuning.sprintSeconds > 0.0f ? seconds / tuning.sprintSeconds : 1.0f;
            state.sprintBlend = std::clamp(state.sprintBlend + (input.isSprinting ? sprintStep : -sprintStep), 0.0f,
                1.0f);

            const math::Vec2 look{wrapAngle(input.yaw - state.lastYaw), input.pitch - state.lastPitch};
            state.lastYaw = input.yaw;
            state.lastPitch = input.pitch;
            state.sway = state.sway - look * tuning.swayLag;
            state.sway.x = std::clamp(state.sway.x, -tuning.swayLimit, tuning.swayLimit);
            state.sway.y = std::clamp(state.sway.y, -tuning.swayLimit, tuning.swayLimit);
            stepSpring(state.sway, state.swayRate, tuning.swayStiffness, tuning.swayDamping, seconds);
            state.breathePhase = wrapAngle(state.breathePhase + 2.0f * math::k_pi * tuning.breatheRate * seconds);

            const auto shots = static_cast<uint8_t>(input.shotCount - state.shotCount);
            state.shotCount = input.shotCount;

            if (input.canFire && !input.isLowered)
            {
                const float crouchScale = input.isCrouched ? tuning.crouchRecoilScale : 1.0f;
                state.recoilRate += tuning.recoilKick * crouchScale * static_cast<float>(shots);
            }

            stepSpring(state.recoil, state.recoilRate, tuning.recoilStiffness, tuning.recoilDamping, seconds);
            state.recoil = std::clamp(state.recoil, -k_maxRecoil, k_maxRecoil);
            updateMovementReaction(state, input, seconds);
            updateAirReaction(state, input, seconds);
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

        AnchoredPose turnedReadyPose(const ViewModelTuning& tuning, float pitch, float yaw, float roll,
            const math::Vec3& offset)
        {
            const AnchoredPose ready = anchored(k_fpReadyPlacement);
            const math::Quat tilt = math::angleAxis(yaw, k_viewUp) * math::angleAxis(-pitch, k_viewRight)
                * math::angleAxis(roll, k_viewBack);

            return AnchoredPose{.rotation = tilt * ready.rotation,
                .anchor = tuning.loweredPivot + tilt * (ready.anchor - tuning.loweredPivot) + offset};
        }

        AnchoredPose loweredPose(const ViewModelTuning& tuning)
        {
            return turnedReadyPose(tuning, tuning.loweredPitch, tuning.loweredYaw, tuning.loweredRoll,
                tuning.loweredOffset);
        }

        AnchoredPose sprintPose(const ViewModelTuning& tuning)
        {
            return turnedReadyPose(tuning, tuning.sprintPitch, tuning.sprintYaw, tuning.sprintRoll, tuning.sprintOffset);
        }

        math::Transform viewModelTransform(const ViewModelState& state, const ViewModelInput& input)
        {
            const ViewModelTuning& tuning = state.tuning;
            const float aimed = smoothstep(state.aimBlend);
            const float motion = 1.0f + (tuning.aimDamping - 1.0f) * aimed;

            const float raised = smoothstep(state.raiseBlend);
            const AnchoredPose up = blendPoses(anchored(k_fpReadyPlacement), anchored(k_fpSightsPlacement), aimed);
            const float sprinting = smoothstep(state.sprintBlend);
            const AnchoredPose pose = blendPoses(blendPoses(loweredPose(tuning), up, raised), sprintPose(tuning),
                sprinting);
            const math::Vec3 anchor = pose.anchor;
            math::Quat rotation = pose.rotation;
            math::Vec3 origin = anchor - rotation * k_weaponAnchor;

            const float bob = tuning.bobAmount * input.bobStrength * motion;
            const float roll = std::sin(input.bobPhase) * (tuning.bobRoll * input.bobStrength
                + tuning.sprintRollSwing * sprinting) - state.tilt - state.sway.x * tuning.swayRoll;
            const float swing = std::sin(input.bobPhase) * tuning.sprintSwing * sprinting + state.moveAngle.y;
            const float climb = state.recoil + state.moveAngle.x + state.air.y;
            const math::Quat kick = math::angleAxis(swing * motion, k_viewUp)
                * math::angleAxis(climb * motion, k_viewRight) * math::angleAxis(roll * motion, k_viewBack);
            origin = anchor + kick * (origin - anchor);
            rotation = kick * rotation;

            const float dip = std::sin(math::k_pi * aimed) * tuning.aimDip;
            const math::Vec3 push{std::sin(input.bobPhase) * bob,
                -(1.0f - std::cos(2.0f * input.bobPhase)) * 0.5f * bob * k_bobVertical - dip,
                state.recoil * motion * tuning.recoilBack + dip * 0.5f};
            origin = origin + math::lerp(tuning.readyOffset, tuning.aimOffset, aimed) + clampLength(push, k_maxPush)
                + state.moveOffset * motion + k_viewUp * (state.air.x * motion)
                + k_viewUp * (-(1.0f - std::cos(2.0f * input.bobPhase)) * 0.5f * tuning.sprintBounce * sprinting);

            const float breathe = tuning.breatheAmount * motion;
            const math::Vec2 breath{std::sin(state.breathePhase) * breathe * 0.6f,
                std::sin(2.0f * state.breathePhase + 0.7f) * breathe};
            const math::Quat sway = math::angleAxis((state.sway.x + breath.x) * motion, k_viewUp)
                * math::angleAxis((state.sway.y + breath.y) * motion, k_viewRight);
            origin = sway * origin;
            rotation = sway * rotation;

            const math::Quat camera = math::angleAxis(input.yaw + input.cameraYawOffset, k_viewUp)
                * math::angleAxis(input.pitch + input.cameraPitchOffset, k_viewRight)
                * math::angleAxis(-input.cameraRoll, k_viewBack);

            return math::Transform{.position = input.eye + camera * origin, .rotation = camera * rotation};
        }
    } //namespace

    lockstep::Result<FirstPersonModel> createFirstPersonModel(CharacterModel&& character, std::string_view aimClip)
    {
        const anim::SkinnedModel& skinned = character.skinned();
        const std::optional<uint32_t> aim = anim::findClip(skinned.clips, aimClip);

        if (!aim)
        {
            return lockstep::Error{lockstep::ErrorCode::InvalidArgument, "viewmodel '" + skinned.name + "' has no clip '"
                + std::string(aimClip) + "'"};
        }

        FirstPersonModel model{.character = std::move(character), .aimClip = *aim};
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
        ImGui::Text("Aim %.2f  raise %.2f  sprint %.2f  sway %.3f %.3f  recoil %.3f", state.aimBlend, state.raiseBlend,
            state.sprintBlend, state.sway.x, state.sway.y, state.recoil);
        ImGui::SliderFloat("Aim time (s)", &tuning.aimSeconds, 0.0f, 0.6f);
        ImGui::SliderFloat("Raise time (s)", &tuning.raiseSeconds, 0.0f, 1.0f);
        ImGui::SliderFloat("Aim dip (m)", &tuning.aimDip, 0.0f, 0.05f);
        ImGui::SliderFloat("Sway lag", &tuning.swayLag, 0.0f, 0.5f);
        ImGui::SliderFloat("Sway limit (rad)", &tuning.swayLimit, 0.0f, 0.2f);
        ImGui::SliderFloat("Sway stiffness", &tuning.swayStiffness, 10.0f, 400.0f);
        ImGui::SliderFloat("Sway damping", &tuning.swayDamping, 1.0f, 40.0f);
        ImGui::SliderFloat("Sway cant (rad/rad)", &tuning.swayRoll, -2.0f, 2.0f);
        ImGui::SliderFloat("Bob (m)", &tuning.bobAmount, 0.0f, 0.03f);
        ImGui::SliderFloat("Bob roll (rad)", &tuning.bobRoll, 0.0f, 0.05f);
        ImGui::SliderFloat("Breathing (rad)", &tuning.breatheAmount, 0.0f, 0.02f);
        ImGui::SliderFloat("Breathing rate (Hz)", &tuning.breatheRate, 0.05f, 1.0f);
        ImGui::SliderFloat("Recoil kick (rad/s)", &tuning.recoilKick, 0.0f, 4.0f);
        ImGui::SliderFloat("Recoil back (m/rad)", &tuning.recoilBack, 0.0f, 4.0f);
        ImGui::SliderFloat("Recoil stiffness", &tuning.recoilStiffness, 50.0f, 800.0f);
        ImGui::SliderFloat("Recoil damping", &tuning.recoilDamping, 1.0f, 60.0f);
        ImGui::SliderFloat("Recoil scale, crouched", &tuning.crouchRecoilScale, 0.0f, 1.0f);
        ImGui::SliderFloat("Strafe tilt (rad)", &tuning.strafeTilt, 0.0f, 0.3f);
        ImGui::SliderFloat("Strafe shift (m)", &tuning.strafeShift, 0.0f, 0.04f);
        ImGui::SliderFloat("Move kick (m/s per m/s)", &tuning.moveKick, 0.0f, 0.1f);
        ImGui::SliderFloat("Move dip", &tuning.moveDip, 0.0f, 2.0f);
        ImGui::SliderFloat("Move stiffness", &tuning.moveStiffness, 10.0f, 400.0f);
        ImGui::SliderFloat("Move damping", &tuning.moveDamping, 1.0f, 40.0f);
        ImGui::SliderFloat("Move limit (m)", &tuning.moveLimit, 0.0f, 0.08f);
        ImGui::SliderFloat("Move muzzle dip (rad/s per m/s)", &tuning.movePitch, 0.0f, 0.3f);
        ImGui::SliderFloat("Move muzzle trail (rad/s per m/s)", &tuning.moveYaw, -0.3f, 0.3f);
        ImGui::SliderFloat("Move angle limit (rad)", &tuning.moveAngleLimit, 0.0f, 0.2f);
        ImGui::SliderFloat("Jump drop (m/s)", &tuning.jumpDrop, 0.0f, 1.5f);
        ImGui::SliderFloat("Jump muzzle dip (rad/s)", &tuning.jumpPitch, 0.0f, 2.0f);
        ImGui::SliderFloat("Land drop (m/s per m/s)", &tuning.landDrop, 0.0f, 0.3f);
        ImGui::SliderFloat("Land muzzle dip (rad/s per m/s)", &tuning.landPitch, 0.0f, 0.5f);
        ImGui::SliderFloat("Land dip from (m/s)", &tuning.landMinSpeed, 0.0f, 10.0f);
        ImGui::SliderFloat("Jump/land stiffness", &tuning.airStiffness, 10.0f, 400.0f);
        ImGui::SliderFloat("Jump/land damping", &tuning.airDamping, 1.0f, 40.0f);
        ImGui::SliderFloat("Jump/land limit (m)", &tuning.airLimit, 0.0f, 0.12f);
        ImGui::SliderFloat("Aim motion scale", &tuning.aimDamping, 0.0f, 1.0f);
        ImGui::DragFloat3("Ready offset (m)", &tuning.readyOffset.x, 0.002f, -0.1f, 0.1f);
        ImGui::DragFloat3("Aim offset (m)", &tuning.aimOffset.x, 0.002f, -0.05f, 0.05f);
        ImGui::SliderFloat("Lowered pitch (rad)", &tuning.loweredPitch, 0.0f, 1.2f);
        ImGui::SliderFloat("Lowered yaw (rad)", &tuning.loweredYaw, -0.8f, 0.8f);
        ImGui::SliderFloat("Lowered roll (rad)", &tuning.loweredRoll, -0.8f, 0.8f);
        ImGui::DragFloat3("Lowered pivot (m)", &tuning.loweredPivot.x, 0.005f, -0.6f, 0.6f);
        ImGui::DragFloat3("Lowered offset (m)", &tuning.loweredOffset.x, 0.002f, -0.2f, 0.2f);
        ImGui::SliderFloat("Sprint pose time (s)", &tuning.sprintSeconds, 0.0f, 0.6f);
        ImGui::SliderFloat("Sprint pitch (rad)", &tuning.sprintPitch, 0.0f, 1.2f);
        ImGui::SliderFloat("Sprint yaw (rad)", &tuning.sprintYaw, -1.2f, 1.2f);
        ImGui::SliderFloat("Sprint roll (rad)", &tuning.sprintRoll, -1.2f, 1.2f);
        ImGui::DragFloat3("Sprint offset (m)", &tuning.sprintOffset.x, 0.002f, -0.2f, 0.2f);
        ImGui::SliderFloat("Sprint swing (rad)", &tuning.sprintSwing, 0.0f, 0.2f);
        ImGui::SliderFloat("Sprint bounce (m)", &tuning.sprintBounce, 0.0f, 0.05f);
        ImGui::SliderFloat("Sprint roll swing (rad)", &tuning.sprintRollSwing, 0.0f, 0.2f);
        ImGui::TreePop();
    }
#endif
} //namespace game
