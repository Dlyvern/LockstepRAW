#include "Lockstep/Anim/RootMotion.hpp"

#include "Lockstep/Anim/JointEdits.hpp"

#include <cmath>
#include <optional>
#include <vector>

namespace lockstep::anim
{
    namespace
    {
        constexpr math::Vec3 k_modelUp{0.0f, 1.0f, 0.0f};

        float twistAngle(const math::Quat& rotation, const math::Vec3& axis)
        {
            const float along = rotation.x * axis.x + rotation.y * axis.y + rotation.z * axis.z;
            return 2.0f * std::atan2(along, rotation.w);
        }
    } //namespace

    RootJoint makeRootJoint(const Skeleton& skeleton, uint32_t joint)
    {
        if (joint >= skeleton.joints.size())
        {
            return RootJoint{};
        }

        Pose rest;
        resetToRest(skeleton, rest);
        std::vector<math::Mat4> matrices(skeleton.joints.size(), math::Mat4{1.0f});
        computeJointMatrices(skeleton, rest, matrices);
        const uint32_t parent = skeleton.joints[joint].parent;
        const math::Quat parentRotation = parent < matrices.size() ? jointRotation(matrices[parent])
                                                                   : jointRotation(skeleton.rootMatrix);

        return RootJoint{.joint = joint, .up = math::normalize(math::inverse(parentRotation) * k_modelUp)};
    }

    float clipRootYaw(const AnimationClip& clip, const RootJoint& root, float seconds)
    {
        if (root.joint == k_noJoint)
        {
            return 0.0f;
        }

        const std::optional<math::Quat> start = sampleJointRotation(clip, root.joint, 0.0f);
        const std::optional<math::Quat> now = sampleJointRotation(clip, root.joint, seconds);

        if (!start || !now)
        {
            return 0.0f;
        }

        return std::remainder(twistAngle(math::normalize(*now * math::inverse(*start)), root.up), 2.0f * math::k_pi);
    }

    void turnRoot(Pose& pose, const RootJoint& root, float yaw)
    {
        if (root.joint >= pose.joints.size())
        {
            return;
        }

        math::Transform& joint = pose.joints[root.joint];
        joint.rotation = math::normalize(math::angleAxis(yaw, root.up) * joint.rotation);
    }
} //namespace lockstep::anim
