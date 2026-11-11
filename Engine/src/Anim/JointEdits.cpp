#include "Lockstep/Anim/JointEdits.hpp"

#include "Lockstep/Core/Rotation.hpp"
#include "Lockstep/Core/Transform.hpp"

#include <algorithm>
#include <array>
#include <cmath>

namespace lockstep::anim
{
    namespace
    {
        constexpr float k_tinyLength{1.0e-5f};
        constexpr float k_maxReach{0.9999f};

        bool isKnown(std::span<const math::Mat4> model, uint32_t joint)
        {
            return joint < model.size();
        }

        void applyEdit(const Skeleton& skeleton, std::span<math::Mat4> model, uint32_t joint, const math::Mat4& edit)
        {
            const std::size_t count = std::min({model.size(), skeleton.joints.size(), std::size_t{k_maxJoints}});

            if (joint >= count)
            {
                return;
            }

            std::array<bool, k_maxJoints> isMoved{};
            isMoved[joint] = true;
            model[joint] = edit * model[joint];

            for (std::size_t index = joint + 1; index < count; ++index)
            {
                const uint32_t parent = skeleton.joints[index].parent;

                if (parent < index && isMoved[parent])
                {
                    isMoved[index] = true;
                    model[index] = edit * model[index];
                }
            }
        }

        float angleBetween(const math::Vec3& from, const math::Vec3& to)
        {
            return std::acos(std::clamp(math::dot(math::normalize(from), math::normalize(to)), -1.0f, 1.0f));
        }

        float lawOfCosines(float adjacentA, float adjacentB, float opposite)
        {
            const float denominator = 2.0f * adjacentA * adjacentB;
            const float cosine = (adjacentA * adjacentA + adjacentB * adjacentB - opposite * opposite) / denominator;
            return std::acos(std::clamp(cosine, -1.0f, 1.0f));
        }
    } //namespace

    math::Vec3 jointPosition(const math::Mat4& matrix) noexcept
    {
        return matrix.getColumn(3).xyz();
    }

    math::Quat jointRotation(const math::Mat4& matrix) noexcept
    {
        math::Mat4 rotation{1.0f};

        for (std::size_t column = 0; column < 3; ++column)
        {
            const math::Vec3 axis = matrix.getColumn(column).xyz();
            const float axisLength = math::length(axis);
            rotation.setColumn(column, math::Vec4(axisLength > 0.0f ? axis / axisLength : axis, 0.0f));
        }

        return math::normalize(math::fromRotationMatrix(rotation));
    }

    void rotateJoints(const Skeleton& skeleton, std::span<math::Mat4> model, uint32_t joint, const math::Quat& rotation,
        const math::Vec3& pivot)
    {
        const math::Mat4 edit = math::toMatrix(math::Transform{.position = pivot - rotation * pivot, .rotation = rotation});
        applyEdit(skeleton, model, joint, edit);
    }

    void translateJoints(const Skeleton& skeleton, std::span<math::Mat4> model, uint32_t joint, const math::Vec3& offset)
    {
        applyEdit(skeleton, model, joint, math::toMatrix(math::Transform{.position = offset}));
    }

    float solveTwoBoneIk(const Skeleton& skeleton, std::span<math::Mat4> model, const TwoBoneChain& chain,
        const math::Vec3& target, const math::Vec3& pole)
    {
        if (!isKnown(model, chain.upper) || !isKnown(model, chain.middle) || !isKnown(model, chain.end))
        {
            return 0.0f;
        }

        const math::Vec3 upper = jointPosition(model[chain.upper]);
        const math::Vec3 middle = jointPosition(model[chain.middle]);
        const math::Vec3 end = jointPosition(model[chain.end]);
        const math::Quat endRotation = jointRotation(model[chain.end]);
        const float upperLength = math::length(middle - upper);
        const float lowerLength = math::length(end - middle);
        const math::Vec3 toEnd = end - upper;
        const math::Vec3 toTarget = target - upper;

        if (upperLength < k_tinyLength || lowerLength < k_tinyLength || math::length(toEnd) < k_tinyLength
            || math::length(toTarget) < k_tinyLength)
        {
            return math::length(end - target);
        }

        const float minReach = std::abs(upperLength - lowerLength) + k_tinyLength;
        const float targetDistance = std::clamp(math::length(toTarget), minReach, (upperLength + lowerLength) * k_maxReach);
        math::Vec3 bendAxis = math::cross(toEnd, middle - upper);

        if (math::length(bendAxis) < k_tinyLength * math::length(toEnd))
        {
            bendAxis = math::cross(toEnd, pole);
        }

        if (math::length(bendAxis) < k_tinyLength)
        {
            return math::length(end - target);
        }

        bendAxis = math::normalize(bendAxis);
        const float upperAngle = angleBetween(toEnd, middle - upper);
        const float middleAngle = angleBetween(upper - middle, end - middle);
        const float wantedUpperAngle = lawOfCosines(upperLength, targetDistance, lowerLength);
        const float wantedMiddleAngle = lawOfCosines(upperLength, lowerLength, targetDistance);

        rotateJoints(skeleton, model, chain.middle, math::angleAxis(wantedMiddleAngle - middleAngle, bendAxis), middle);
        rotateJoints(skeleton, model, chain.upper, math::angleAxis(wantedUpperAngle - upperAngle, bendAxis), upper);
        rotateJoints(skeleton, model, chain.upper, math::rotationBetween(toEnd, toTarget), upper);

        const math::Quat turned = jointRotation(model[chain.end]);
        const math::Vec3 reached = jointPosition(model[chain.end]);
        rotateJoints(skeleton, model, chain.end, math::normalize(endRotation * math::inverse(turned)), reached);

        return math::length(reached - target);
    }
} //namespace lockstep::anim
