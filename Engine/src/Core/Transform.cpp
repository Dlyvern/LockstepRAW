#include "Lockstep/Core/Transform.hpp"

namespace lockstep::math
{
    Mat4 toMatrix(const Transform& transform) noexcept
    {
        return compose(transform.position, transform.rotation, transform.scale);
    }

    Transform interpolate(const Transform& from, const Transform& to, float t) noexcept
    {
        return Transform{
            lerp(from.position, to.position, t),
            slerp(from.rotation, to.rotation, t),
            lerp(from.scale, to.scale, t)};
    }
} //namespace lockstep::math
