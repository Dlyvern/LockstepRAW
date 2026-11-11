#include "Lockstep/Core/Rotation.hpp"

#include <algorithm>
#include <cmath>

namespace lockstep::math
{
    namespace
    {
        constexpr float k_tiny{1.0e-8f};

        Vec3 anyPerpendicular(const Vec3& direction)
        {
            const Vec3 axis = std::abs(direction.x) < 0.9f ? Vec3{1.0f, 0.0f, 0.0f} : Vec3{0.0f, 1.0f, 0.0f};
            return normalize(cross(direction, axis));
        }
    } //namespace

    Quat rotationBetween(const Vec3& from, const Vec3& to) noexcept
    {
        const float fromLength = length(from);
        const float toLength = length(to);

        if (fromLength < k_tiny || toLength < k_tiny)
        {
            return Quat{};
        }

        const Vec3 start = from / fromLength;
        const Vec3 end = to / toLength;
        const float cosine = dot(start, end);

        if (cosine < -0.9999f)
        {
            return angleAxis(k_pi, anyPerpendicular(start));
        }

        const Vec3 axis = cross(start, end);
        return normalize(Quat{axis.x, axis.y, axis.z, 1.0f + cosine});
    }

    Vec3 toScaledAngleAxis(const Quat& rotation) noexcept
    {
        const Quat shortest = rotation.w < 0.0f ? -rotation : rotation;
        const Vec3 vector{shortest.x, shortest.y, shortest.z};
        const float sine = length(vector);

        if (sine < k_tiny)
        {
            return vector * 2.0f;
        }

        const float angle = 2.0f * std::atan2(sine, std::clamp(shortest.w, -1.0f, 1.0f));
        return vector * (angle / sine);
    }

    Quat fromScaledAngleAxis(const Vec3& scaledAxis) noexcept
    {
        const float angle = length(scaledAxis);

        if (angle < k_tiny)
        {
            return normalize(Quat{scaledAxis.x * 0.5f, scaledAxis.y * 0.5f, scaledAxis.z * 0.5f, 1.0f});
        }

        return angleAxis(angle, scaledAxis / angle);
    }

    Quat clampAngle(const Quat& rotation, float maxAngle) noexcept
    {
        const Vec3 scaled = toScaledAngleAxis(rotation);
        const float angle = length(scaled);

        if (angle <= maxAngle || angle < k_tiny)
        {
            return rotation;
        }

        return fromScaledAngleAxis(scaled * (std::max(maxAngle, 0.0f) / angle));
    }
} //namespace lockstep::math
