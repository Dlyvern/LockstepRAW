#include "Lockstep/Physics/PhysicsWorld.hpp"

#include "PhysXConversions.hpp"
#include "PhysXCooking.hpp"
#include "PhysicsState.hpp"

#include <cmath>
#include <string>

namespace lockstep::physics
{
    namespace
    {
        constexpr float k_minimumScale{1.0e-6f};

        Status validateScale(const math::Vec3& scale, const char* what)
        {
            for (int axis = 0; axis < 3; ++axis)
            {
                if (!std::isfinite(scale[axis]) || std::abs(scale[axis]) < k_minimumScale)
                {
                    return Error{ErrorCode::InvalidArgument, std::string(what) + ": scale must be non-zero"};
                }
            }

            return {};
        }

        Result<BodyId> addStaticMeshBody(PhysicsState& state, physx::PxTriangleMesh& mesh,
            const math::Transform& transform, const char* what)
        {
            const physx::PxTriangleMeshGeometry geometry(&mesh, physx::PxMeshScale(toPx(transform.scale)),
                physx::PxMeshGeometryFlag::eDOUBLE_SIDED);
            physx::PxRigidStatic* actor = state.physics->createRigidStatic(toPx(transform));
            physx::PxShape* shape = actor != nullptr
                ? physx::PxRigidActorExt::createExclusiveShape(*actor, geometry, *state.defaultMaterial)
                : nullptr;

            if (shape == nullptr)
            {
                if (actor != nullptr)
                {
                    actor->release();
                }

                return Error{ErrorCode::PhysicsFailure,
                    state.errors.describeFailure((std::string(what) + ": actor or shape creation failed").c_str())};
            }

            const auto key = state.bodies.insert(actor);

            if (!key)
            {
                actor->release();
                return Error{ErrorCode::OutOfMemory, std::string(what) + ": no more body handles"};
            }

            actor->userData = encodeActorTag(ActorKind::Body, key->index);
            state.scene->addActor(*actor);

            return BodyId{.index = key->index, .generation = key->generation};
        }
    } //namespace

    Status validateMeshData(std::span<const math::Vec3> positions, std::span<const uint32_t> indices, const char* what)
    {
        if (positions.empty() || indices.empty() || indices.size() % 3 != 0)
        {
            return Error{ErrorCode::InvalidArgument, std::string(what) + ": needs vertices and a whole number of triangles"};
        }

        for (const uint32_t index : indices)
        {
            if (index >= positions.size())
            {
                return Error{ErrorCode::InvalidArgument, std::string(what) + ": index out of range"};
            }
        }

        return {};
    }

    physx::PxTriangleMeshDesc describeTriangleMesh(std::span<const math::Vec3> positions,
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

        return description;
    }

    physx::PxCookingParams meshCookingParams(const physx::PxTolerancesScale& scale)
    {
        physx::PxCookingParams parameters(scale);
        parameters.meshEdgeLengthMaxLimit = 0.0f;

        return parameters;
    }

    Result<BodyId> PhysicsWorld::createStaticMesh(std::span<const math::Vec3> positions,
        std::span<const uint32_t> indices, const math::Transform& transform)
    {
        constexpr const char* k_what = "PhysicsWorld::createStaticMesh";

        if (auto valid = validateMeshData(positions, indices, k_what); !valid)
        {
            return valid.error();
        }

        if (auto valid = validateScale(transform.scale, k_what); !valid)
        {
            return valid.error();
        }

        PhysicsState& state = *m_state;
        physx::PxTriangleMesh* mesh = PxCreateTriangleMesh(meshCookingParams(state.physics->getTolerancesScale()),
            describeTriangleMesh(positions, indices), state.physics->getPhysicsInsertionCallback());

        if (mesh == nullptr)
        {
            return Error{ErrorCode::PhysicsFailure, state.errors.describeFailure("PhysicsWorld::createStaticMesh: cooking failed")};
        }

        auto body = addStaticMeshBody(state, *mesh, transform, k_what);
        mesh->release();

        return body;
    }

    Result<std::vector<BodyId>> PhysicsWorld::createStaticMeshes(std::span<const std::byte> cooked,
        std::span<const math::Transform> transforms)
    {
        constexpr const char* k_what = "PhysicsWorld::createStaticMeshes";

        for (const math::Transform& transform : transforms)
        {
            if (auto valid = validateScale(transform.scale, k_what); !valid)
            {
                return valid.error();
            }
        }

        PhysicsState& state = *m_state;
        physx::PxDefaultMemoryInputData input(const_cast<physx::PxU8*>(reinterpret_cast<const physx::PxU8*>(cooked.data())),
            static_cast<physx::PxU32>(cooked.size()));
        physx::PxTriangleMesh* mesh = cooked.empty() ? nullptr : state.physics->createTriangleMesh(input);

        if (mesh == nullptr)
        {
            return Error{ErrorCode::PhysicsFailure,
                state.errors.describeFailure("PhysicsWorld::createStaticMeshes: the cooked mesh could not be loaded")};
        }

        std::vector<BodyId> bodies;
        bodies.reserve(transforms.size());

        for (const math::Transform& transform : transforms)
        {
            auto body = addStaticMeshBody(state, *mesh, transform, k_what);

            if (!body)
            {
                for (const BodyId created : bodies)
                {
                    destroyBody(created);
                }

                mesh->release();
                return body.error();
            }

            bodies.push_back(body.value());
        }

        mesh->release();

        return bodies;
    }
} //namespace lockstep::physics
