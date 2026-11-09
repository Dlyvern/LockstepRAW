#ifndef LOCKSTEP_PHYSICS_PHYSX_CONVERSIONS_HPP
#define LOCKSTEP_PHYSICS_PHYSX_CONVERSIONS_HPP

#include "Lockstep/Core/Transform.hpp"

#include <PxPhysicsAPI.h>

namespace lockstep::physics
{
    // Returns: `vector` as a PhysX vector.
    [[nodiscard]] inline physx::PxVec3 toPx(const math::Vec3& vector) noexcept
    {
        return physx::PxVec3(vector.x, vector.y, vector.z);
    }

    // Returns: `rotation` as a PhysX quaternion.
    [[nodiscard]] inline physx::PxQuat toPx(const math::Quat& rotation) noexcept
    {
        return physx::PxQuat(rotation.x, rotation.y, rotation.z, rotation.w);
    }

    // Returns: position and rotation of `transform` as a PhysX pose; scale is dropped.
    [[nodiscard]] inline physx::PxTransform toPx(const math::Transform& transform) noexcept
    {
        return physx::PxTransform(toPx(transform.position), toPx(transform.rotation).getNormalized());
    }

    // Returns: `position` as a PhysX double-precision controller position.
    [[nodiscard]] inline physx::PxExtendedVec3 toPxExtended(const math::Vec3& position) noexcept
    {
        return physx::PxExtendedVec3(position.x, position.y, position.z);
    }

    // Returns: `vector` as an Engine vector.
    [[nodiscard]] inline math::Vec3 fromPx(const physx::PxVec3& vector) noexcept
    {
        return math::Vec3{vector.x, vector.y, vector.z};
    }

    // Returns: `position` as an Engine vector, narrowed to float.
    [[nodiscard]] inline math::Vec3 fromPx(const physx::PxExtendedVec3& position) noexcept
    {
        return math::Vec3{static_cast<float>(position.x), static_cast<float>(position.y), static_cast<float>(position.z)};
    }

    // Returns: `pose` as an Engine transform with scale 1.
    [[nodiscard]] inline math::Transform fromPx(const physx::PxTransform& pose) noexcept
    {
        return math::Transform{
            .position = fromPx(pose.p),
            .rotation = math::Quat(pose.q.x, pose.q.y, pose.q.z, pose.q.w),
            .scale = math::Vec3{1.0f, 1.0f, 1.0f}};
    }
} //namespace lockstep::physics

#endif //LOCKSTEP_PHYSICS_PHYSX_CONVERSIONS_HPP
