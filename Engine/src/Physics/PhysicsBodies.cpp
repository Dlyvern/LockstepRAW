#include "Lockstep/Physics/PhysicsWorld.hpp"

#include "PhysXConversions.hpp"
#include "PhysicsState.hpp"

#include <type_traits>

namespace lockstep::physics
{
    namespace
    {
        const physx::PxQuat k_capsuleXToY{physx::PxHalfPi, physx::PxVec3(0.0f, 0.0f, 1.0f)};

        struct ShapeGeometry
        {
            physx::PxGeometryHolder geometry;
            physx::PxTransform localPose{physx::PxIdentity};
        };

        std::optional<ShapeGeometry> toGeometry(const BodyShape& shape)
        {
            return std::visit([](const auto& value) -> std::optional<ShapeGeometry>
            {
                using Shape = std::decay_t<decltype(value)>;

                if constexpr (std::is_same_v<Shape, BoxShape>)
                {
                    if (!(value.halfExtents.x > 0.0f && value.halfExtents.y > 0.0f && value.halfExtents.z > 0.0f))
                    {
                        return std::nullopt;
                    }

                    return ShapeGeometry{.geometry = physx::PxBoxGeometry(toPx(value.halfExtents))};
                }
                else if constexpr (std::is_same_v<Shape, SphereShape>)
                {
                    if (!(value.radius > 0.0f))
                    {
                        return std::nullopt;
                    }

                    return ShapeGeometry{.geometry = physx::PxSphereGeometry(value.radius)};
                }
                else if constexpr (std::is_same_v<Shape, CapsuleShape>)
                {
                    if (!(value.radius > 0.0f && value.halfHeight > 0.0f))
                    {
                        return std::nullopt;
                    }

                    return ShapeGeometry{
                        .geometry = physx::PxCapsuleGeometry(value.radius, value.halfHeight),
                        .localPose = physx::PxTransform(k_capsuleXToY)};
                }
                else
                {
                    return ShapeGeometry{.geometry = physx::PxPlaneGeometry()};
                }
            }, shape);
        }

        physx::PxTransform planePose(const PlaneShape& plane)
        {
            const physx::PxVec3 normal = toPx(plane.normal).getNormalized();
            return physx::PxTransformFromPlaneEquation(physx::PxPlane(normal, -plane.distance));
        }

        Status validate(const BodyDescription& description)
        {
            if (const auto* plane = std::get_if<PlaneShape>(&description.shape))
            {
                if (description.type != BodyType::Static)
                {
                    return Error{ErrorCode::InvalidArgument, "PhysicsWorld::createBody: a plane must be static"};
                }

                if (!(math::lengthSquared(plane->normal) > 0.0f))
                {
                    return Error{ErrorCode::InvalidArgument, "PhysicsWorld::createBody: plane normal is zero"};
                }
            }

            if (description.type == BodyType::Dynamic && !(description.density > 0.0f))
            {
                return Error{ErrorCode::InvalidArgument, "PhysicsWorld::createBody: density must be > 0"};
            }

            return {};
        }

        physx::PxRigidActor* createActor(physx::PxPhysics& physics, const BodyDescription& description)
        {
            if (const auto* plane = std::get_if<PlaneShape>(&description.shape))
            {
                return physics.createRigidStatic(planePose(*plane));
            }

            const physx::PxTransform pose = toPx(description.transform);

            if (description.type == BodyType::Dynamic)
            {
                return physics.createRigidDynamic(pose);
            }

            return physics.createRigidStatic(pose);
        }
    } //namespace

    Result<BodyId> PhysicsWorld::createBody(const BodyDescription& description)
    {
        if (auto valid = validate(description); !valid)
        {
            return valid.error();
        }

        const std::optional<ShapeGeometry> geometry = toGeometry(description.shape);

        if (!geometry)
        {
            return Error{ErrorCode::InvalidArgument, "PhysicsWorld::createBody: shape sizes must be > 0"};
        }

        PhysicsState& state = *m_state;
        physx::PxMaterial* material = state.physics->createMaterial(description.friction, description.friction,
            description.restitution);

        if (material == nullptr)
        {
            return Error{ErrorCode::PhysicsFailure, state.errors.describeFailure("PhysicsWorld::createBody: createMaterial failed")};
        }

        physx::PxRigidActor* actor = createActor(*state.physics, description);
        physx::PxShape* shape = actor != nullptr
            ? physx::PxRigidActorExt::createExclusiveShape(*actor, geometry->geometry.any(), *material)
            : nullptr;
        material->release();

        if (shape == nullptr)
        {
            if (actor != nullptr)
            {
                actor->release();
            }

            return Error{ErrorCode::PhysicsFailure, state.errors.describeFailure("PhysicsWorld::createBody: actor or shape creation failed")};
        }

        shape->setLocalPose(geometry->localPose);

        if (auto* dynamic = actor->is<physx::PxRigidDynamic>())
        {
            physx::PxRigidBodyExt::updateMassAndInertia(*dynamic, description.density);
        }

        const auto key = state.bodies.insert(actor);

        if (!key)
        {
            actor->release();
            return Error{ErrorCode::OutOfMemory, "PhysicsWorld::createBody: no more body handles"};
        }

        actor->userData = encodeActorTag(ActorKind::Body, key->index);
        state.scene->addActor(*actor);

        return BodyId{.index = key->index, .generation = key->generation};
    }

    bool PhysicsWorld::destroyBody(BodyId body)
    {
        const auto actor = m_state->bodies.remove({body.index, body.generation});

        if (!actor)
        {
            return false;
        }

        (*actor)->release();

        return true;
    }

    bool PhysicsWorld::isAlive(BodyId body) const noexcept
    {
        return m_state->findBody(body) != nullptr;
    }

    std::optional<math::Transform> PhysicsWorld::transform(BodyId body) const
    {
        const physx::PxRigidActor* actor = m_state->findBody(body);

        if (actor == nullptr)
        {
            return std::nullopt;
        }

        return fromPx(actor->getGlobalPose());
    }

    bool PhysicsWorld::setTransform(BodyId body, const math::Transform& transform)
    {
        physx::PxRigidActor* actor = m_state->findBody(body);

        if (actor == nullptr)
        {
            return false;
        }

        actor->setGlobalPose(toPx(transform));

        return true;
    }

    std::optional<math::Vec3> PhysicsWorld::linearVelocity(BodyId body) const
    {
        const physx::PxRigidActor* actor = m_state->findBody(body);

        if (actor == nullptr)
        {
            return std::nullopt;
        }

        const auto* dynamic = actor->is<physx::PxRigidDynamic>();

        return dynamic != nullptr ? fromPx(dynamic->getLinearVelocity()) : math::Vec3{0.0f, 0.0f, 0.0f};
    }

    std::optional<math::Vec3> PhysicsWorld::angularVelocity(BodyId body) const
    {
        const physx::PxRigidActor* actor = m_state->findBody(body);

        if (actor == nullptr)
        {
            return std::nullopt;
        }

        const auto* dynamic = actor->is<physx::PxRigidDynamic>();

        return dynamic != nullptr ? fromPx(dynamic->getAngularVelocity()) : math::Vec3{0.0f, 0.0f, 0.0f};
    }

    bool PhysicsWorld::setVelocity(BodyId body, const math::Vec3& linear, const math::Vec3& angular)
    {
        physx::PxRigidActor* actor = m_state->findBody(body);
        auto* dynamic = actor != nullptr ? actor->is<physx::PxRigidDynamic>() : nullptr;

        if (dynamic == nullptr)
        {
            return false;
        }

        dynamic->setLinearVelocity(toPx(linear));
        dynamic->setAngularVelocity(toPx(angular));

        return true;
    }
} //namespace lockstep::physics
