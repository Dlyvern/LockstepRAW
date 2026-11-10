#include "Lockstep/Anim/DrivenPose.hpp"

#include <algorithm>
#include <cassert>

namespace lockstep::anim
{
    namespace
    {
        const DrivenJoint* findDriven(std::span<const DrivenJoint> driven, std::size_t joint)
        {
            for (const DrivenJoint& entry : driven)
            {
                if (entry.joint == joint)
                {
                    return &entry;
                }
            }

            return nullptr;
        }

        math::Vec3 scaleOf(const math::Mat4& matrix)
        {
            return math::Vec3{math::length(matrix.getColumn(0).xyz()), math::length(matrix.getColumn(1).xyz()),
                math::length(matrix.getColumn(2).xyz())};
        }
    } //namespace

    void computeDrivenJointMatrices(const Skeleton& skeleton, const Pose& pose, const math::Transform& placement,
        std::span<const DrivenJoint> driven, std::span<math::Mat4> model)
    {
        assert(pose.joints.size() == skeleton.joints.size() && model.size() == skeleton.joints.size());

        const std::size_t count = std::min({skeleton.joints.size(), pose.joints.size(), model.size()});
        const math::Mat4 worldToModel = math::inverse(math::toMatrix(placement));

        for (std::size_t index = 0; index < count; ++index)
        {
            const uint32_t parent = skeleton.joints[index].parent;
            const math::Mat4& parentMatrix = parent < index ? model[parent] : skeleton.rootMatrix;
            model[index] = parentMatrix * math::toMatrix(pose.joints[index]);

            if (const DrivenJoint* entry = findDriven(driven, index))
            {
                model[index] = worldToModel
                    * math::toMatrix(math::Transform{.position = entry->world.position,
                        .rotation = entry->world.rotation,
                        .scale = scaleOf(model[index])});
            }
        }
    }

    math::Mat4 blendJointMatrix(const math::Mat4& from, const math::Mat4& to, float weight)
    {
        math::Transform first{};
        math::Transform second{};

        if (!math::decompose(from, first.position, first.rotation, first.scale)
            || !math::decompose(to, second.position, second.rotation, second.scale))
        {
            return weight < 0.5f ? from : to;
        }

        return math::toMatrix(math::interpolate(first, second, std::clamp(weight, 0.0f, 1.0f)));
    }
} //namespace lockstep::anim
