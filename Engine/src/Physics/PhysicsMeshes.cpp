#include "Lockstep/Physics/PhysicsWorld.hpp"

#include "PhysXConversions.hpp"
#include "PhysicsState.hpp"

#include <cmath>

namespace lockstep::physics
{
    namespace
    {
        constexpr float k_minimumScale{1.0e-6f};

        Status validateMesh(std::span<const math::Vec3> positions, std::span<const uint32_t> indices,
            const math::Vec3& scale)
        {
            if (positions.empty() || indices.empty() || indices.size() % 3 != 0)
            {
                return Error{ErrorCode::InvalidArgument,
                    "PhysicsWorld::createStaticMesh: needs vertices and a whole number of triangles"};
            }

            for (const uint32_t index : indices)
            {
                if (index >= positions.size())
                {
                    return Error{ErrorCode::InvalidArgument, "PhysicsWorld::createStaticMesh: index out of range"};
                }
            }

            for (int axis = 0; axis < 3; ++axis)
            {
                if (!std::isfinite(scale[axis]) || std::abs(scale[axis]) < k_minimumScale)
                {
                    return Error{ErrorCode::InvalidArgument, "PhysicsWorld::createStaticMesh: scale must be non-zero"};
                }
            }

            return {};
        }

        physx::PxTriangleMesh* cookTriangleMesh(physx::PxPhysics& physics, std::span<const math::Vec3> positions,
            std::span<const uint32_t> indices)
        {
            static_assert(sizeof(math::Vec3) == sizeof(physx::PxVec3));

            physx::PxTriangleMeshDesc description;
            description.points.count = static_cast<physx::PxU32>(positions.size());
            description.points.stride = sizeof(math::Vec3);
            description.points.data = positions.data();
            description.triangles.count = static_cast<physx::PxU32>(indices.size() / 3);
            description.triangles.stride = 3 * sizeof(uint32_t);
            description.triangles.data = indices.data();

            physx::PxCookingParams parameters(physics.getTolerancesScale());
            parameters.meshEdgeLengthMaxLimit = 0.0f;

            return PxCreateTriangleMesh(parameters, description, physics.getPhysicsInsertionCallback());
        }
    } //namespace

    Result<BodyId> PhysicsWorld::createStaticMesh(std::span<const math::Vec3> positions,
        std::span<const uint32_t> indices, const math::Transform& transform)
    {
        if (auto valid = validateMesh(positions, indices, transform.scale); !valid)
        {
            return valid.error();
        }

        PhysicsState& state = *m_state;
        physx::PxTriangleMesh* mesh = cookTriangleMesh(*state.physics, positions, indices);

        if (mesh == nullptr)
        {
            return Error{ErrorCode::PhysicsFailure,
                state.errors.describeFailure("PhysicsWorld::createStaticMesh: cooking failed")};
        }

        const physx::PxTriangleMeshGeometry geometry(mesh, physx::PxMeshScale(toPx(transform.scale)),
            physx::PxMeshGeometryFlag::eDOUBLE_SIDED);
        physx::PxRigidStatic* actor = state.physics->createRigidStatic(toPx(transform));
        physx::PxShape* shape = actor != nullptr
            ? physx::PxRigidActorExt::createExclusiveShape(*actor, geometry, *state.defaultMaterial)
            : nullptr;
        mesh->release();

        if (shape == nullptr)
        {
            if (actor != nullptr)
            {
                actor->release();
            }

            return Error{ErrorCode::PhysicsFailure,
                state.errors.describeFailure("PhysicsWorld::createStaticMesh: actor or shape creation failed")};
        }

        const auto key = state.bodies.insert(actor);

        if (!key)
        {
            actor->release();
            return Error{ErrorCode::OutOfMemory, "PhysicsWorld::createStaticMesh: no more body handles"};
        }

        actor->userData = encodeActorTag(ActorKind::Body, key->index);
        state.scene->addActor(*actor);

        return BodyId{.index = key->index, .generation = key->generation};
    }
} //namespace lockstep::physics
