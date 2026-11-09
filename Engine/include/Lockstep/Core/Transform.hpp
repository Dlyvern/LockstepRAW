#ifndef LOCKSTEP_CORE_TRANSFORM_HPP
#define LOCKSTEP_CORE_TRANSFORM_HPP

#include "Lockstep/Core/Math.hpp"

namespace lockstep::math
{
    // Position, rotation and scale of an object in world space. Applied as scale, then rotation,
    // then translation. Default is the identity transform.
    struct Transform
    {
        Vec3 position{0.0f, 0.0f, 0.0f};
        Quat rotation{};
        Vec3 scale{1.0f, 1.0f, 1.0f};
    };

    // Builds the model matrix of a transform.
    // transform: the transform to convert.
    // Returns:   a column-major matrix applying scale, then rotation, then translation.
    [[nodiscard]] Mat4 toMatrix(const Transform& transform) noexcept;

    // Blends between two transforms. Used to render between two fixed simulation ticks.
    // from:    the transform at the earlier tick.
    // to:      the transform at the later tick.
    // t:       blend factor, 0 gives `from` and 1 gives `to`.
    // Returns: position and scale blended linearly, rotation blended along the shortest arc.
    [[nodiscard]] Transform interpolate(const Transform& from, const Transform& to, float t) noexcept;
} //namespace lockstep::math

#endif //LOCKSTEP_CORE_TRANSFORM_HPP
