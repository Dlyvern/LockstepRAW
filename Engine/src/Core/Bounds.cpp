#include "Lockstep/Core/Bounds.hpp"

namespace lockstep::math
{
    bool isEmpty(const Aabb& box) noexcept
    {
        return box.min.x > box.max.x || box.min.y > box.max.y || box.min.z > box.max.z;
    }

    void expand(Aabb& box, const Vec3& point) noexcept
    {
        box.min = min(box.min, point);
        box.max = max(box.max, point);
    }

    void expand(Aabb& box, const Aabb& other) noexcept
    {
        if (isEmpty(other))
        {
            return;
        }

        expand(box, other.min);
        expand(box, other.max);
    }

    Vec3 center(const Aabb& box) noexcept
    {
        return (box.min + box.max) * 0.5f;
    }

    Vec3 size(const Aabb& box) noexcept
    {
        return box.max - box.min;
    }

    Aabb transformBounds(const Aabb& box, const Transform& transform) noexcept
    {
        Aabb result{};

        if (isEmpty(box))
        {
            return result;
        }

        const Mat4 matrix = toMatrix(transform);

        for (int corner = 0; corner < 8; ++corner)
        {
            const Vec3 point{
                (corner & 1) != 0 ? box.max.x : box.min.x,
                (corner & 2) != 0 ? box.max.y : box.min.y,
                (corner & 4) != 0 ? box.max.z : box.min.z};
            expand(result, transformPoint(matrix, point));
        }

        return result;
    }
} //namespace lockstep::math
