#include "Lockstep/Physics/MeshCooker.hpp"

#include "PhysXCooking.hpp"
#include "PhysXErrorReporter.hpp"

#include <cstring>

namespace lockstep::physics
{
    struct MeshCookerState
    {
        MeshCookerState() = default;

        ~MeshCookerState()
        {
            if (foundation != nullptr)
            {
                foundation->release();
            }
        }

        MeshCookerState(const MeshCookerState&) = delete;
        MeshCookerState& operator=(const MeshCookerState&) = delete;

        physx::PxDefaultAllocator allocator;
        PhysXErrorReporter errors;
        physx::PxFoundation* foundation{nullptr};
        physx::PxTolerancesScale scale{};
    };

    Result<MeshCooker> MeshCooker::create()
    {
        auto state = std::make_unique<MeshCookerState>();
        state->foundation = PxCreateFoundation(PX_PHYSICS_VERSION, state->allocator, state->errors);

        if (state->foundation == nullptr)
        {
            return Error{ErrorCode::PhysicsFailure,
                state->errors.describeFailure("MeshCooker: PxCreateFoundation failed (is a PhysicsWorld alive?)")};
        }

        return MeshCooker(std::move(state));
    }

    MeshCooker::MeshCooker(std::unique_ptr<MeshCookerState> state) noexcept
        : m_state(std::move(state))
    {
    }

    MeshCooker::~MeshCooker() = default;

    MeshCooker::MeshCooker(MeshCooker&& other) noexcept = default;

    MeshCooker& MeshCooker::operator=(MeshCooker&& other) noexcept = default;

    Result<std::vector<std::byte>> MeshCooker::cook(std::span<const math::Vec3> positions,
        std::span<const uint32_t> indices) const
    {
        if (auto valid = validateMeshData(positions, indices, "MeshCooker::cook"); !valid)
        {
            return valid.error();
        }

        const physx::PxTriangleMeshDesc description = describeTriangleMesh(positions, indices);
        physx::PxDefaultMemoryOutputStream stream(m_state->allocator);

        if (!PxCookTriangleMesh(meshCookingParams(m_state->scale), description, stream))
        {
            return Error{ErrorCode::PhysicsFailure, m_state->errors.describeFailure("MeshCooker::cook: cooking failed")};
        }

        std::vector<std::byte> cooked(stream.getSize());
        std::memcpy(cooked.data(), stream.getData(), cooked.size());

        return cooked;
    }
} //namespace lockstep::physics
