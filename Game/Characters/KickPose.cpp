#include "Characters/KickPose.hpp"

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

        constexpr math::Vec3 k_modelSide{1.0f, 0.0f, 0.0f};
        constexpr math::Vec3 k_modelUp{0.0f, 1.0f, 0.0f};
        constexpr float k_chamberShare{0.85f};
        constexpr float k_retractBend{0.6f};
        constexpr float k_minSeconds{0.001f};

        float smoothstep(float value)
        {
            const float clamped = std::clamp(value, 0.0f, 1.0f);
            return clamped * clamped * (3.0f - 2.0f * clamped);
        }

        struct LegAngles
        {
            float thigh{0.0f};
            float knee{0.0f};
        };

        LegAngles legAngles(const KickPoseTuning& tuning, float seconds)
        {
            const float strike = std::max(tuning.strikeSeconds, tuning.chamberSeconds + k_minSeconds);
            const float release = strike + tuning.holdSeconds;

            if (seconds < tuning.chamberSeconds)
            {
                const float chamber = smoothstep(seconds / std::max(tuning.chamberSeconds, k_minSeconds));
                return LegAngles{.thigh = tuning.thighLift * k_chamberShare * chamber,
                    .knee = tuning.chamberKnee * chamber};
            }

            if (seconds < strike)
            {
                const float extend = smoothstep((seconds - tuning.chamberSeconds) / (strike - tuning.chamberSeconds));
                return LegAngles{.thigh = tuning.thighLift * (k_chamberShare + (1.0f - k_chamberShare) * extend),
                    .knee = tuning.chamberKnee + (tuning.strikeKnee - tuning.chamberKnee) * extend};
            }

            if (seconds < release)
            {
                return LegAngles{.thigh = tuning.thighLift, .knee = tuning.strikeKnee};
            }

            const float retract = smoothstep((seconds - release) / std::max(tuning.seconds - release, k_minSeconds));
            return LegAngles{.thigh = tuning.thighLift * (1.0f - retract),
                .knee = tuning.strikeKnee * (1.0f - retract) + tuning.chamberKnee * k_retractBend
                    * std::sin(math::k_pi * retract)};
        }

        void append(std::vector<anim::JointRotationOffset>& offsets, uint32_t joint, const math::Quat& rotation)
        {
            if (joint != anim::k_noJoint)
            {
                offsets.push_back(anim::JointRotationOffset{.joint = joint, .rotation = rotation});
            }
        }
    } //namespace

    float appendKickOffsets(std::vector<anim::JointRotationOffset>& offsets, const ReactionRig& rig,
        const KickPoseTuning& tuning, float seconds)
    {
        if (seconds < 0.0f || seconds > tuning.seconds || rig.model == nullptr)
        {
            return 0.0f;
        }

        const LegAngles leg = legAngles(tuning, seconds);
        const ReactionRegionJoints& right = rig.regions[static_cast<std::size_t>(ReactionRegion::LegRight)];
        const ReactionRegionJoints& torso = rig.regions[static_cast<std::size_t>(ReactionRegion::Torso)];
        const float sign = tuning.forwardSign;
        const float extension = leg.thigh / std::max(tuning.thighLift, k_minSeconds);
        append(offsets, right.joints[0].joint, math::angleAxis(sign * tuning.thighInward * extension, k_modelUp)
            * math::angleAxis(-sign * leg.thigh, k_modelSide));
        append(offsets, right.joints[1].joint, math::angleAxis(sign * leg.knee, k_modelSide));

        for (const ReactionJoint& joint : torso.joints)
        {
            append(offsets, joint.joint, math::angleAxis(-sign * tuning.spineLean * extension * joint.share, k_modelSide));
        }

        return extension;
    }

#if LOCKSTEP_DEV_TOOLS
    void kickPoseDevSection(KickPoseTuning& tuning)
    {
        if (!ImGui::TreeNode("Kick pose"))
        {
            return;
        }

        ImGui::SliderFloat("Length (s)", &tuning.seconds, 0.1f, 1.5f);
        ImGui::SliderFloat("Chamber (s)", &tuning.chamberSeconds, 0.0f, 0.5f);
        ImGui::SliderFloat("Strike at (s)", &tuning.strikeSeconds, 0.0f, 0.8f);
        ImGui::SliderFloat("Hold (s)", &tuning.holdSeconds, 0.0f, 0.5f);
        ImGui::SliderFloat("Thigh lift (rad)", &tuning.thighLift, 0.0f, 2.2f);
        ImGui::SliderFloat("Chamber knee (rad)", &tuning.chamberKnee, 0.0f, 2.5f);
        ImGui::SliderFloat("Strike knee (rad)", &tuning.strikeKnee, 0.0f, 1.5f);
        ImGui::SliderFloat("Upper body lean (rad)", &tuning.spineLean, -0.8f, 0.8f);
        ImGui::SliderFloat("Thigh inward (rad)", &tuning.thighInward, -0.8f, 0.8f);
        ImGui::SliderFloat("Forward sign", &tuning.forwardSign, -1.0f, 1.0f);
        ImGui::TreePop();
    }
#endif
} //namespace game
