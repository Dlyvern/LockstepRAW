#include "Npcs/HitReactions.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <string>
#include <string_view>

#if LOCKSTEP_DEV_TOOLS
#include <imgui.h>
#endif

namespace game
{
    namespace
    {
        namespace anim = lockstep::anim;

        constexpr math::Vec3 k_up{0.0f, 1.0f, 0.0f};
        constexpr math::Vec3 k_modelRight{1.0f, 0.0f, 0.0f};
        constexpr float k_maxStep{1.0f / 120.0f};
        constexpr float k_tinyAngle{1.0e-5f};
        constexpr float k_tinyLength{1.0e-3f};
        constexpr float k_headReach{1.6f};
        constexpr float k_minStrength{0.25f};
        constexpr float k_maxStrength{2.0f};
        constexpr float k_bodyStagger{0.6f};
        constexpr float k_minStagger{-0.3f};
        constexpr float k_maxStagger{1.3f};
        constexpr float k_buckledKnee{1.6f};
        constexpr float k_steadyKnee{0.4f};
        constexpr float k_thighShare{0.5f};
        constexpr std::array<float, k_reactionRegionCount> k_regionRadius{0.16f, 0.1f, 0.05f, 0.05f, 0.07f, 0.07f};

        struct RegionNames
        {
            std::string_view pivot;
            std::string_view tip;
            std::array<std::string_view, 3> joints;
            std::array<float, 3> shares;
        };

        constexpr std::array<RegionNames, k_reactionRegionCount> k_regionNames{{
            {"Hips", "Neck", {"Spine", "Spine1", "Spine2"}, {0.3f, 0.35f, 0.35f}},
            {"Neck", "Head", {"Neck", "Head", ""}, {0.4f, 0.6f, 0.0f}},
            {"LeftArm", "LeftHand", {"LeftShoulder", "LeftArm", ""}, {0.35f, 0.65f, 0.0f}},
            {"RightArm", "RightHand", {"RightShoulder", "RightArm", ""}, {0.35f, 0.65f, 0.0f}},
            {"LeftUpLeg", "LeftFoot", {"LeftUpLeg", "LeftLeg", ""}, {0.8f, 0.2f, 0.0f}},
            {"RightUpLeg", "RightFoot", {"RightUpLeg", "RightLeg", ""}, {0.8f, 0.2f, 0.0f}},
        }};

        bool isNamed(std::string_view name, std::string_view bone)
        {
            if (name == bone)
            {
                return true;
            }

            if (name.size() <= bone.size() || !name.ends_with(bone))
            {
                return false;
            }

            const char separator = name[name.size() - bone.size() - 1];
            return separator == ':' || separator == '_';
        }

        uint32_t findBone(const anim::Skeleton& skeleton, std::string_view bone)
        {
            if (bone.empty())
            {
                return anim::k_noJoint;
            }

            for (std::size_t index = 0; index < skeleton.joints.size(); ++index)
            {
                if (isNamed(skeleton.joints[index].name, bone))
                {
                    return static_cast<uint32_t>(index);
                }
            }

            return anim::k_noJoint;
        }

        math::Vec3 jointPosition(std::span<const math::Mat4> joints, uint32_t joint)
        {
            return joint < joints.size() ? math::transformPoint(joints[joint], math::Vec3{0.0f, 0.0f, 0.0f})
                                         : math::Vec3{0.0f, 0.0f, 0.0f};
        }

        float segmentDistance(const math::Vec3& point, const math::Vec3& from, const math::Vec3& to)
        {
            const math::Vec3 along = to - from;
            const float lengthSquared = math::lengthSquared(along);
            const float t = lengthSquared > 0.0f ? std::clamp(math::dot(point - from, along) / lengthSquared, 0.0f, 1.0f)
                                                 : 0.0f;
            return math::length(point - (from + along * t));
        }

        bool hasRegion(const ReactionRig& rig, std::size_t region)
        {
            return rig.regions[region].pivot != anim::k_noJoint && rig.regions[region].tip != anim::k_noJoint;
        }

        ReactionRegion nearestRegion(const ReactionRig& rig, std::span<const math::Mat4> joints, const math::Vec3& point)
        {
            ReactionRegion nearest = ReactionRegion::Torso;
            float nearestDistance = std::numeric_limits<float>::max();

            for (std::size_t region = 0; region < k_reactionRegionCount; ++region)
            {
                if (!hasRegion(rig, region))
                {
                    continue;
                }

                const math::Vec3 pivot = jointPosition(joints, rig.regions[region].pivot);
                math::Vec3 tip = jointPosition(joints, rig.regions[region].tip);

                if (region == static_cast<std::size_t>(ReactionRegion::Head))
                {
                    tip = pivot + (tip - pivot) * k_headReach;
                }

                const float distance = segmentDistance(point, pivot, tip) - k_regionRadius[region];

                if (distance < nearestDistance)
                {
                    nearestDistance = distance;
                    nearest = static_cast<ReactionRegion>(region);
                }
            }

            return nearest;
        }

        math::Vec3 torqueAxis(const math::Vec3& lever, const math::Vec3& direction)
        {
            const math::Vec3 arm = math::lengthSquared(lever) > k_tinyLength * k_tinyLength ? math::normalize(lever) : k_up;
            const math::Vec3 axis = math::cross(arm, direction);
            return math::lengthSquared(axis) > k_tinyLength ? axis : math::cross(k_up, direction);
        }

        void kick(ReactionSpring& spring, const math::Vec3& axis, float speed)
        {
            spring.velocity = spring.velocity + axis * speed;
        }

        void stepSpring(ReactionSpring& spring, float stiffness, float damping, float step, float maxAngle)
        {
            const math::Vec3 acceleration = spring.angle * -stiffness - spring.velocity * damping;
            spring.velocity = spring.velocity + acceleration * step;
            spring.angle = spring.angle + spring.velocity * step;
            const float angle = math::length(spring.angle);

            if (angle > maxAngle)
            {
                spring.angle = spring.angle * (maxAngle / angle);
            }
        }

        void addOffset(HitReaction& reaction, uint32_t joint, const math::Vec3& axis, float angle)
        {
            if (joint == anim::k_noJoint || std::abs(angle) < k_tinyAngle)
            {
                return;
            }

            reaction.offsets.push_back(anim::JointRotationOffset{.joint = joint, .rotation = math::angleAxis(angle, axis)});
        }

        void addStaggerOffsets(HitReaction& reaction, const ReactionRig& rig, const HitReactionTuning& tuning)
        {
            const float fold = reaction.stagger * tuning.staggerFold;
            const ReactionRegionJoints& torso = rig.regions[static_cast<std::size_t>(ReactionRegion::Torso)];

            for (const ReactionJoint& joint : torso.joints)
            {
                addOffset(reaction, joint.joint, k_modelRight, fold * joint.share);
            }

            const std::array<float, 2> sides{-1.0f, 1.0f};
            const std::array<ReactionRegion, 2> legs{ReactionRegion::LegLeft, ReactionRegion::LegRight};

            for (std::size_t side = 0; side < sides.size(); ++side)
            {
                const float buckle = reaction.staggerSide == 0.0f ? 1.0f
                    : reaction.staggerSide == sides[side] ? k_buckledKnee : k_steadyKnee;
                const float knee = fold * buckle;
                addOffset(reaction, rig.regions[static_cast<std::size_t>(legs[side])].pivot, k_modelRight,
                    -knee * k_thighShare);
                addOffset(reaction, rig.knees[side], k_modelRight, knee);
            }
        }
    } //namespace

    void resolveReactionRig(ReactionRig& rig, const anim::SkinnedModel& model)
    {
        if (rig.model == &model)
        {
            return;
        }

        rig = ReactionRig{.model = &model};

        for (std::size_t region = 0; region < k_reactionRegionCount; ++region)
        {
            const RegionNames& names = k_regionNames[region];
            ReactionRegionJoints& joints = rig.regions[region];
            joints.pivot = findBone(model.skeleton, names.pivot);
            joints.tip = findBone(model.skeleton, names.tip);

            for (std::size_t index = 0; index < names.joints.size(); ++index)
            {
                joints.joints[index] = ReactionJoint{.joint = findBone(model.skeleton, names.joints[index]),
                    .share = names.shares[index]};
            }
        }

        rig.knees = {findBone(model.skeleton, "LeftLeg"), findBone(model.skeleton, "RightLeg")};
    }

    ReactionRegion pushHitReaction(HitReaction& reaction, const ReactionRig& rig, std::span<const math::Mat4> joints,
        const math::Transform& placement, const ReactionHit& hit, const HitReactionTuning& tuning)
    {
        const math::Quat toModel = math::conjugate(placement.rotation);
        const math::Vec3 turned = toModel * (hit.point - placement.position);
        const math::Vec3 point{turned.x / placement.scale.x, turned.y / placement.scale.y, turned.z / placement.scale.z};
        const math::Vec3 direction = math::normalize(toModel * hit.direction);
        const float damage = hit.damage > 0 ? static_cast<float>(hit.damage) : tuning.referenceDamage;
        const float strength = std::clamp(damage / std::max(tuning.referenceDamage, 1.0f), k_minStrength, k_maxStrength);
        const ReactionRegion region = nearestRegion(rig, joints, point);
        const auto index = static_cast<std::size_t>(region);
        const auto torso = static_cast<std::size_t>(ReactionRegion::Torso);
        const float gain = region == ReactionRegion::Head ? tuning.headshotScale : 1.0f;

        if (hasRegion(rig, index))
        {
            const math::Vec3 lever = point - jointPosition(joints, rig.regions[index].pivot);
            kick(reaction.springs[index], torqueAxis(lever, direction), tuning.impulse * strength * gain);
        }

        if (region != ReactionRegion::Torso && hasRegion(rig, torso))
        {
            const math::Vec3 lever = point - jointPosition(joints, rig.regions[torso].pivot);
            kick(reaction.springs[torso], torqueAxis(lever, direction), tuning.impulse * strength * tuning.torsoShare);
        }

        const bool isLeg = region == ReactionRegion::LegLeft || region == ReactionRegion::LegRight;

        if (isLeg || damage >= tuning.staggerDamage)
        {
            reaction.staggerSpeed += tuning.staggerImpulse * strength * (isLeg ? 1.0f : k_bodyStagger);
            reaction.staggerSide = region == ReactionRegion::LegLeft ? -1.0f : region == ReactionRegion::LegRight ? 1.0f : 0.0f;
        }

        return region;
    }

    void updateHitReaction(HitReaction& reaction, const ReactionRig& rig, const HitReactionTuning& tuning,
        float seconds)
    {
        reaction.offsets.clear();

        if (!tuning.isEnabled)
        {
            resetHitReaction(reaction);
            return;
        }

        const float omega = 2.0f * math::k_pi * std::max(tuning.frequency, 0.1f);
        const float stiffness = omega * omega;
        const float damping = 2.0f * std::clamp(tuning.damping, 0.0f, 2.0f) * omega;
        float left = std::max(seconds, 0.0f);

        while (left > 0.0f)
        {
            const float step = std::min(left, k_maxStep);
            left -= step;

            for (ReactionSpring& spring : reaction.springs)
            {
                stepSpring(spring, stiffness, damping, step, tuning.maxAngle);
            }

            const float acceleration = -stiffness * reaction.stagger - damping * reaction.staggerSpeed;
            reaction.staggerSpeed += acceleration * step;
            reaction.stagger = std::clamp(reaction.stagger + reaction.staggerSpeed * step, k_minStagger, k_maxStagger);
        }

        for (std::size_t region = 0; region < k_reactionRegionCount; ++region)
        {
            const math::Vec3& angle = reaction.springs[region].angle;
            const float magnitude = math::length(angle);

            if (magnitude < k_tinyAngle)
            {
                continue;
            }

            const math::Vec3 axis = angle * (1.0f / magnitude);

            for (const ReactionJoint& joint : rig.regions[region].joints)
            {
                addOffset(reaction, joint.joint, axis, magnitude * joint.share);
            }
        }

        addStaggerOffsets(reaction, rig, tuning);
    }

    void resetHitReaction(HitReaction& reaction)
    {
        reaction.springs = {};
        reaction.stagger = 0.0f;
        reaction.staggerSpeed = 0.0f;
        reaction.staggerSide = 0.0f;
        reaction.offsets.clear();
    }

#if LOCKSTEP_DEV_TOOLS
    void hitReactionDevSection(HitReactionTuning& tuning)
    {
        if (!ImGui::TreeNode("Hit reactions"))
        {
            return;
        }

        ImGui::Checkbox("Enabled##reactions", &tuning.isEnabled);
        ImGui::SliderFloat("Spring frequency (Hz)", &tuning.frequency, 0.5f, 6.0f);
        ImGui::SliderFloat("Damping ratio", &tuning.damping, 0.1f, 1.5f);
        ImGui::SliderFloat("Impulse (rad/s)", &tuning.impulse, 0.0f, 20.0f);
        ImGui::SliderFloat("Torso share of limb hits", &tuning.torsoShare, 0.0f, 1.0f);
        ImGui::SliderFloat("Headshot scale", &tuning.headshotScale, 0.0f, 3.0f);
        ImGui::SliderFloat("Max angle (rad)", &tuning.maxAngle, 0.05f, 1.5f);
        ImGui::SliderFloat("Reference damage", &tuning.referenceDamage, 1.0f, 150.0f);
        ImGui::SliderFloat("Stagger damage", &tuning.staggerDamage, 1.0f, 150.0f);
        ImGui::SliderFloat("Stagger impulse", &tuning.staggerImpulse, 0.0f, 30.0f);
        ImGui::SliderFloat("Stagger fold (rad)", &tuning.staggerFold, 0.0f, 1.0f);
        ImGui::TreePop();
    }
#endif
} //namespace game
