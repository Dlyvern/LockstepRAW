#ifndef LOCKSTEP_PHYSICS_MESH_COOKER_HPP
#define LOCKSTEP_PHYSICS_MESH_COOKER_HPP

#include "Lockstep/Core/Math.hpp"
#include "Lockstep/Core/Result.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <vector>

namespace lockstep::physics
{
    struct MeshCookerState;

    // Cooks triangle meshes into PhysX's serialized form offline (LockstepCooker), so the game
    // loads collision with PhysicsWorld::createStaticMeshes() instead of cooking at load time.
    // Uses the same cooking settings as PhysicsWorld::createStaticMesh(). Owns a PhysX foundation,
    // so it can't exist alongside a PhysicsWorld (PhysX allows one foundation per process).
    class MeshCooker
    {
    public:
        // Creates the PhysX foundation the cooker needs.
        // Returns: the cooker, or PhysicsFailure if PhysX could not be initialised (also while a
        //          PhysicsWorld or another MeshCooker exists).
        [[nodiscard]] static Result<MeshCooker> create();

        // Releases the foundation.
        ~MeshCooker();

        // Takes over `other`. `other` becomes empty and may only be destroyed.
        MeshCooker(MeshCooker&& other) noexcept;

        // Releases this cooker, then takes over `other`.
        MeshCooker& operator=(MeshCooker&& other) noexcept;

        MeshCooker(const MeshCooker&) = delete;
        MeshCooker& operator=(const MeshCooker&) = delete;

        // Cooks one triangle mesh. The same input gives the same bytes. Thread-safe.
        // positions: mesh-space vertex positions.
        // indices:   triangle list into `positions`; a multiple of 3, every index in range.
        // Returns:   the cooked mesh, or InvalidArgument for bad data, or PhysicsFailure if
        //            cooking failed (e.g. every triangle was degenerate).
        [[nodiscard]] Result<std::vector<std::byte>> cook(std::span<const math::Vec3> positions,
            std::span<const uint32_t> indices) const;

    private:
        explicit MeshCooker(std::unique_ptr<MeshCookerState> state) noexcept;

        std::unique_ptr<MeshCookerState> m_state;
    };
} //namespace lockstep::physics

#endif //LOCKSTEP_PHYSICS_MESH_COOKER_HPP
