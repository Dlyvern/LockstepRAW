#include "Lockstep/Physics/PhysicsWorld.hpp"

#include "PhysXConversions.hpp"
#include "PhysicsState.hpp"

namespace lockstep::physics
{
    namespace
    {
        const physx::PxHitFlags k_hitFlags{physx::PxHitFlag::ePOSITION | physx::PxHitFlag::eNORMAL};

        class BodiesOnlyFilter final : public physx::PxQueryFilterCallback
        {
        public:
            explicit BodiesOnlyFilter(const physx::PxRigidActor* ignoredBody) noexcept
                : m_ignoredBody(ignoredBody)
            {
            }

            physx::PxQueryHitType::Enum preFilter(const physx::PxFilterData&, const physx::PxShape*,
                const physx::PxRigidActor* actor, physx::PxHitFlags&) override
            {
                const bool isBody = actor != nullptr && decodeActorTag(actor->userData).kind == ActorKind::Body;
                const bool isHittable = isBody && actor != m_ignoredBody;
                return isHittable ? physx::PxQueryHitType::eBLOCK : physx::PxQueryHitType::eNONE;
            }

            physx::PxQueryHitType::Enum postFilter(const physx::PxFilterData&, const physx::PxQueryHit&,
                const physx::PxShape*, const physx::PxRigidActor*) override
            {
                return physx::PxQueryHitType::eBLOCK;
            }

        private:
            const physx::PxRigidActor* m_ignoredBody;
        };

        struct Ray
        {
            physx::PxVec3 origin;
            physx::PxVec3 direction;
            float maxDistance;
        };

        std::optional<RaycastHit> raycastBodies(const PhysicsState& state, const Ray& ray, BodyId ignoredBody)
        {
            BodiesOnlyFilter filter(state.findBody(ignoredBody));
            const physx::PxQueryFilterData filterData(
                physx::PxQueryFlag::eSTATIC | physx::PxQueryFlag::eDYNAMIC | physx::PxQueryFlag::ePREFILTER);
            physx::PxRaycastBuffer result;

            if (!state.scene->raycast(ray.origin, ray.direction, ray.maxDistance, result, k_hitFlags, filterData, &filter)
                || !result.hasBlock || result.block.actor == nullptr)
            {
                return std::nullopt;
            }

            const auto key = state.bodies.keyAt(decodeActorTag(result.block.actor->userData).index);

            if (!key)
            {
                return std::nullopt;
            }

            return RaycastHit{
                .body = BodyId{.index = key->index, .generation = key->generation},
                .point = fromPx(result.block.position),
                .normal = fromPx(result.block.normal),
                .distance = result.block.distance};
        }

        std::optional<physx::PxGeomRaycastHit> raycastCapsule(const physx::PxCapsuleController& controller, const Ray& ray)
        {
            const physx::PxCapsuleGeometry capsule(controller.getRadius(), 0.5f * controller.getHeight());
            const physx::PxQuat xToUp = physx::PxShortestRotation(physx::PxVec3(1.0f, 0.0f, 0.0f), controller.getUpDirection());
            const physx::PxExtendedVec3 center = controller.getPosition();
            const physx::PxTransform pose(physx::PxVec3(static_cast<float>(center.x), static_cast<float>(center.y),
                static_cast<float>(center.z)), xToUp);
            physx::PxGeomRaycastHit hit;

            if (physx::PxGeometryQuery::raycast(ray.origin, ray.direction, capsule, pose, ray.maxDistance, k_hitFlags, 1, &hit) == 0)
            {
                return std::nullopt;
            }

            return hit;
        }

        std::optional<RaycastHit> raycastControllers(const PhysicsState& state, const Ray& ray, ControllerId ignored)
        {
            std::optional<RaycastHit> closest;

            for (uint32_t index = 0; index < state.controllers.capacity(); ++index)
            {
                const auto key = state.controllers.keyAt(index);

                if (!key || (key->index == ignored.index && key->generation == ignored.generation))
                {
                    continue;
                }

                const auto hit = raycastCapsule(*state.controllers.find(*key)->controller, ray);

                if (hit && (!closest || hit->distance < closest->distance))
                {
                    closest = RaycastHit{
                        .controller = ControllerId{.index = key->index, .generation = key->generation},
                        .point = fromPx(hit->position),
                        .normal = fromPx(hit->normal),
                        .distance = hit->distance};
                }
            }

            return closest;
        }
    } //namespace

    std::optional<RaycastHit> PhysicsWorld::raycast(const math::Vec3& origin, const math::Vec3& direction,
        float maxDistance, const RaycastIgnore& ignore) const
    {
        const Ray ray{.origin = toPx(origin), .direction = toPx(direction).getNormalized(), .maxDistance = maxDistance};

        if (ray.direction.isZero() || !(maxDistance > 0.0f))
        {
            return std::nullopt;
        }

        const std::optional<RaycastHit> bodyHit = raycastBodies(*m_state, ray, ignore.body);
        const std::optional<RaycastHit> controllerHit = raycastControllers(*m_state, ray, ignore.controller);

        if (bodyHit && controllerHit)
        {
            return controllerHit->distance < bodyHit->distance ? controllerHit : bodyHit;
        }

        return bodyHit ? bodyHit : controllerHit;
    }
} //namespace lockstep::physics
