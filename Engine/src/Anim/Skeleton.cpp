#include "Lockstep/Anim/Skeleton.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>

namespace lockstep::anim
{
    namespace
    {
        float largestScale(const math::Mat4& matrix)
        {
            float largest = 0.0f;

            for (std::size_t column = 0; column < 3; ++column)
            {
                const math::Vec4 axis = matrix.getColumn(column);
                largest = std::max(largest, axis.x * axis.x + axis.y * axis.y + axis.z * axis.z);
            }

            return std::sqrt(largest);
        }
    } //namespace

    void resetToRest(const Skeleton& skeleton, Pose& pose)
    {
        pose.joints.resize(skeleton.joints.size());

        for (std::size_t index = 0; index < skeleton.joints.size(); ++index)
        {
            pose.joints[index] = skeleton.joints[index].rest;
        }
    }

    void computeJointMatrices(const Skeleton& skeleton, const Pose& pose, std::span<math::Mat4> model)
    {
        assert(pose.joints.size() == skeleton.joints.size() && model.size() == skeleton.joints.size());

        const std::size_t count = std::min({skeleton.joints.size(), pose.joints.size(), model.size()});

        for (std::size_t index = 0; index < count; ++index)
        {
            const uint32_t parent = skeleton.joints[index].parent;
            const math::Mat4& parentMatrix = parent < index ? model[parent] : skeleton.rootMatrix;
            model[index] = parentMatrix * math::toMatrix(pose.joints[index]);
        }
    }

    void computeSkinMatrices(const Skeleton& skeleton, std::span<const math::Mat4> model, std::span<math::Mat4> skin)
    {
        const std::size_t count = std::min({skeleton.joints.size(), model.size(), skin.size()});

        for (std::size_t index = 0; index < count; ++index)
        {
            skin[index] = model[index] * skeleton.joints[index].inverseBind;
        }
    }

    math::Aabb poseBounds(std::span<const math::Mat4> model, std::span<const math::Mat4> skin,
        std::span<const float> radii)
    {
        math::Aabb bounds{};
        const std::size_t count = std::min({model.size(), skin.size(), radii.size()});

        for (std::size_t index = 0; index < count; ++index)
        {
            if (radii[index] <= 0.0f)
            {
                continue;
            }

            const math::Vec4 origin = model[index].getColumn(3);
            const math::Vec3 centre{origin.x, origin.y, origin.z};
            const float radius = radii[index] * largestScale(skin[index]);
            const math::Vec3 reach{radius, radius, radius};
            math::expand(bounds, centre - reach);
            math::expand(bounds, centre + reach);
        }

        return bounds;
    }

    uint32_t findJoint(const Skeleton& skeleton, std::string_view name)
    {
        for (std::size_t index = 0; index < skeleton.joints.size(); ++index)
        {
            if (skeleton.joints[index].name == name)
            {
                return static_cast<uint32_t>(index);
            }
        }

        return k_noJoint;
    }
} //namespace lockstep::anim
