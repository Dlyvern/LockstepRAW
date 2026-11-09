#ifndef LOCKSTEP_PHYSICS_PHYSX_COOKING_HPP
#define LOCKSTEP_PHYSICS_PHYSX_COOKING_HPP

#include "Lockstep/Core/Math.hpp"
#include "Lockstep/Core/Result.hpp"

#include <PxPhysicsAPI.h>

#include <cstdint>
#include <span>

namespace lockstep::physics
{
    // Checks triangle-mesh data before cooking.
    // positions: mesh-space vertex positions.
    // indices:   triangle list into `positions`.
    // what:      the calling function, for the message.
    // Returns:   success, or InvalidArgument for no triangles, a partial triangle or an index out
    //            of range.
    [[nodiscard]] Status validateMeshData(std::span<const math::Vec3> positions, std::span<const uint32_t> indices,
        const char* what);

    // positions: mesh-space vertex positions (they must outlive the description).
    // indices:   triangle list into `positions` (same).
    // Returns:   the PhysX description of the mesh.
    [[nodiscard]] physx::PxTriangleMeshDesc describeTriangleMesh(std::span<const math::Vec3> positions,
        std::span<const uint32_t> indices);

    // The cooking settings every triangle mesh uses, at runtime and offline, so meshes cooked by
    // LockstepCooker match those cooked in the game: large triangles are accepted without
    // PhysX's size warning (level floors and walls are meant to be large).
    // scale:   the tolerances the PhysicsWorld is created with.
    // Returns: the parameters.
    [[nodiscard]] physx::PxCookingParams meshCookingParams(const physx::PxTolerancesScale& scale);
} //namespace lockstep::physics

#endif //LOCKSTEP_PHYSICS_PHYSX_COOKING_HPP
