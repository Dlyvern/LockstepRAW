#ifndef LOCKSTEP_CORE_ROTATION_HPP
#define LOCKSTEP_CORE_ROTATION_HPP

#include "Lockstep/Core/Math.hpp"

namespace lockstep::math
{
    // The shortest rotation that turns one direction into another.
    // from:    the start direction; need not be normalised.
    // to:      the end direction; need not be normalised.
    // Returns: the rotation, identity when either direction is zero. Opposite directions turn
    //          half a circle about any axis perpendicular to them.
    [[nodiscard]] Quat rotationBetween(const Vec3& from, const Vec3& to) noexcept;

    // A rotation as its axis scaled by its angle (the quaternion logarithm times two), taking the
    // shorter way round. Such vectors add, scale and decay like positions, which springs and
    // inertialization offsets need.
    // rotation: a unit quaternion.
    // Returns:  axis * angle in radians, angle in [0, pi].
    [[nodiscard]] Vec3 toScaledAngleAxis(const Quat& rotation) noexcept;

    // The inverse of toScaledAngleAxis().
    // scaledAxis: axis * angle in radians.
    // Returns:    the unit quaternion.
    [[nodiscard]] Quat fromScaledAngleAxis(const Vec3& scaledAxis) noexcept;

    // Limits how far a rotation turns, keeping its axis.
    // rotation: a unit quaternion.
    // maxAngle: the largest angle allowed, radians.
    // Returns:  the rotation, scaled down to maxAngle if it turns further.
    [[nodiscard]] Quat clampAngle(const Quat& rotation, float maxAngle) noexcept;
} //namespace lockstep::math

#endif //LOCKSTEP_CORE_ROTATION_HPP
