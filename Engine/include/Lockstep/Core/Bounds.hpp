#ifndef LOCKSTEP_CORE_BOUNDS_HPP
#define LOCKSTEP_CORE_BOUNDS_HPP

#include "Lockstep/Core/Math.hpp"
#include "Lockstep/Core/Transform.hpp"

namespace lockstep::math
{
    // Axis-aligned bounding box. The default box is empty (min above max) and grows to fit the
    // first point added to it.
    struct Aabb
    {
        Vec3 min{1.0e30f, 1.0e30f, 1.0e30f};
        Vec3 max{-1.0e30f, -1.0e30f, -1.0e30f};
    };

    // box:     a box.
    // Returns: true if nothing has been added to it.
    [[nodiscard]] bool isEmpty(const Aabb& box) noexcept;

    // Grows a box to contain a point.
    // box:   the box to grow.
    // point: the point.
    void expand(Aabb& box, const Vec3& point) noexcept;

    // Grows a box to contain another box. An empty `other` changes nothing.
    // box:   the box to grow.
    // other: the box to contain.
    void expand(Aabb& box, const Aabb& other) noexcept;

    // box:     a non-empty box.
    // Returns: its centre.
    [[nodiscard]] Vec3 center(const Aabb& box) noexcept;

    // box:     a non-empty box.
    // Returns: its size along each axis.
    [[nodiscard]] Vec3 size(const Aabb& box) noexcept;

    // Moves a box into another space, keeping it axis-aligned.
    // box:       a box in local space.
    // transform: local-to-target transform.
    // Returns:   the smallest axis-aligned box containing the transformed box (empty if `box` is).
    [[nodiscard]] Aabb transformBounds(const Aabb& box, const Transform& transform) noexcept;
} //namespace lockstep::math

#endif //LOCKSTEP_CORE_BOUNDS_HPP
