#include "Lockstep/Physics/PhysicsWorld.hpp"

#include "PhysXConversions.hpp"
#include "PhysicsState.hpp"

#include <algorithm>
#include <cmath>

namespace lockstep::physics
{
    namespace
    {
        constexpr float k_minMoveDistance{1.0e-4f};
        const physx::PxVec3 k_upDirection{0.0f, 1.0f, 0.0f};

        class IgnoreSimulatedBodiesFilter final : public physx::PxQueryFilterCallback
        {
        public:
            physx::PxQueryHitType::Enum preFilter(const physx::PxFilterData&, const physx::PxShape*,
                const physx::PxRigidActor* actor, physx::PxHitFlags&) override
            {
                const auto* dynamic = actor != nullptr ? actor->is<physx::PxRigidDynamic>() : nullptr;
                const bool isSimulated = dynamic != nullptr
                    && !dynamic->getRigidBodyFlags().isSet(physx::PxRigidBodyFlag::eKINEMATIC);
                return isSimulated ? physx::PxQueryHitType::eNONE : physx::PxQueryHitType::eBLOCK;
            }

            physx::PxQueryHitType::Enum postFilter(const physx::PxFilterData&, const physx::PxQueryHit&,
                const physx::PxShape*, const physx::PxRigidActor*) override
            {
                return physx::PxQueryHitType::eBLOCK;
            }
        };

        class StandingRoomFilter final : public physx::PxQueryFilterCallback
        {
        public:
            physx::PxQueryHitType::Enum preFilter(const physx::PxFilterData&, const physx::PxShape*,
                const physx::PxRigidActor* actor, physx::PxHitFlags&) override
            {
                if (actor == nullptr || decodeActorTag(actor->userData).kind == ActorKind::Controller)
                {
                    return physx::PxQueryHitType::eNONE;
                }

                const auto* dynamic = actor->is<physx::PxRigidDynamic>();
                const bool isSimulated = dynamic != nullptr
                    && !dynamic->getRigidBodyFlags().isSet(physx::PxRigidBodyFlag::eKINEMATIC);
                return isSimulated ? physx::PxQueryHitType::eNONE : physx::PxQueryHitType::eBLOCK;
            }

            physx::PxQueryHitType::Enum postFilter(const physx::PxFilterData&, const physx::PxQueryHit&,
                const physx::PxShape*, const physx::PxRigidActor*) override
            {
                return physx::PxQueryHitType::eBLOCK;
            }
        };

        bool fitsCapsule(const PhysicsState& state, const physx::PxCapsuleController& controller, float cylinderHeight)
        {
            const float skin = controller.getContactOffset();
            const float radius = std::max(controller.getRadius() - skin, k_minMoveDistance);
            const physx::PxExtendedVec3 foot = controller.getFootPosition();
            const float centreHeight = 2.0f * skin + radius + 0.5f * cylinderHeight;
            const physx::PxVec3 centre(static_cast<float>(foot.x), static_cast<float>(foot.y) + centreHeight,
                static_cast<float>(foot.z));
            const physx::PxQuat xToUp = physx::PxShortestRotation(physx::PxVec3(1.0f, 0.0f, 0.0f), k_upDirection);
            const physx::PxCapsuleGeometry capsule(radius, 0.5f * cylinderHeight);
            const physx::PxQueryFilterData filterData(physx::PxQueryFlag::eSTATIC | physx::PxQueryFlag::eDYNAMIC
                | physx::PxQueryFlag::ePREFILTER | physx::PxQueryFlag::eANY_HIT);
            StandingRoomFilter filter;
            physx::PxOverlapBuffer hit;

            return !state.scene->overlap(capsule, physx::PxTransform(centre, xToUp), hit, filterData, &filter);
        }

        Status validate(const ControllerDescription& description)
        {
            const bool isSizeValid = description.radius > 0.0f && description.height > 2.0f * description.radius;
            const bool isSlopeValid = description.slopeLimitDegrees > 0.0f && description.slopeLimitDegrees < 90.0f;

            if (!isSizeValid || !isSlopeValid || !(description.stepOffset >= 0.0f) || !(description.contactOffset > 0.0f))
            {
                return Error{ErrorCode::InvalidArgument,
                    "PhysicsWorld::createController: needs radius > 0, height > 2 * radius, slope limit in (0, 90), "
                    "step offset >= 0 and contact offset > 0"};
            }

            return {};
        }

        physx::PxCapsuleControllerDesc toCapsuleDescription(const ControllerDescription& description, physx::PxMaterial& material)
        {
            physx::PxCapsuleControllerDesc capsule;
            capsule.radius = description.radius;
            capsule.height = description.height - 2.0f * description.radius;
            capsule.stepOffset = description.stepOffset;
            capsule.slopeLimit = std::cos(math::radians(description.slopeLimitDegrees));
            capsule.contactOffset = description.contactOffset;
            capsule.upDirection = k_upDirection;
            capsule.nonWalkableMode = physx::PxControllerNonWalkableMode::ePREVENT_CLIMBING_AND_FORCE_SLIDING;
            capsule.climbingMode = physx::PxCapsuleClimbingMode::eCONSTRAINED;
            capsule.material = &material;

            return capsule;
        }
    } //namespace

    Result<ControllerId> PhysicsWorld::createController(const ControllerDescription& description)
    {
        if (auto valid = validate(description); !valid)
        {
            return valid.error();
        }

        PhysicsState& state = *m_state;
        const physx::PxCapsuleControllerDesc capsule = toCapsuleDescription(description, *state.defaultMaterial);

        if (!capsule.isValid())
        {
            return Error{ErrorCode::InvalidArgument,
                "PhysicsWorld::createController: PhysX rejected the description (step offset too tall?)"};
        }

        auto* controller = static_cast<physx::PxCapsuleController*>(state.controllerManager->createController(capsule));

        if (controller == nullptr)
        {
            return Error{ErrorCode::PhysicsFailure, state.errors.describeFailure("PhysicsWorld::createController failed")};
        }

        controller->setFootPosition(toPxExtended(description.footPosition));
        const auto key = state.controllers.insert(ControllerSlot{.controller = controller});

        if (!key)
        {
            controller->release();
            return Error{ErrorCode::OutOfMemory, "PhysicsWorld::createController: no more controller handles"};
        }

        controller->getActor()->userData = encodeActorTag(ActorKind::Controller, key->index);

        return ControllerId{.index = key->index, .generation = key->generation};
    }

    bool PhysicsWorld::destroyController(ControllerId controller)
    {
        const auto slot = m_state->controllers.remove({controller.index, controller.generation});

        if (!slot)
        {
            return false;
        }

        slot->controller->release();

        return true;
    }

    bool PhysicsWorld::isAlive(ControllerId controller) const noexcept
    {
        return m_state->findController(controller) != nullptr;
    }

    std::optional<ControllerMove> PhysicsWorld::move(ControllerId controller, const math::Vec3& displacement,
        float deltaSeconds)
    {
        ControllerSlot* slot = m_state->findController(controller);

        if (slot == nullptr)
        {
            return std::nullopt;
        }

        IgnoreSimulatedBodiesFilter ignoreSimulatedBodies;
        const physx::PxControllerFilters filters(nullptr, &ignoreSimulatedBodies);
        const physx::PxControllerCollisionFlags flags =
            slot->controller->move(toPx(displacement), k_minMoveDistance, deltaSeconds, filters);

        const ControllerMove result{
            .grounded = flags.isSet(physx::PxControllerCollisionFlag::eCOLLISION_DOWN),
            .hitCeiling = flags.isSet(physx::PxControllerCollisionFlag::eCOLLISION_UP),
            .hitSides = flags.isSet(physx::PxControllerCollisionFlag::eCOLLISION_SIDES)};
        slot->isGrounded = result.grounded;

        return result;
    }

    std::optional<math::Vec3> PhysicsWorld::footPosition(ControllerId controller) const
    {
        const ControllerSlot* slot = m_state->findController(controller);

        if (slot == nullptr)
        {
            return std::nullopt;
        }

        return fromPx(slot->controller->getFootPosition());
    }

    bool PhysicsWorld::setFootPosition(ControllerId controller, const math::Vec3& position)
    {
        ControllerSlot* slot = m_state->findController(controller);

        if (slot == nullptr)
        {
            return false;
        }

        slot->controller->setFootPosition(toPxExtended(position));
        slot->controller->invalidateCache();
        slot->isGrounded = false;

        return true;
    }

    bool PhysicsWorld::setControllerHeight(ControllerId controller, float height, ControllerResize resize)
    {
        ControllerSlot* slot = m_state->findController(controller);

        if (slot == nullptr)
        {
            return false;
        }

        physx::PxCapsuleController& capsule = *slot->controller;
        const float cylinderHeight = height - 2.0f * capsule.getRadius();

        if (!(cylinderHeight > 0.0f))
        {
            return false;
        }

        const bool isGrowing = cylinderHeight > capsule.getHeight();

        if (isGrowing && resize == ControllerResize::MustFit && !fitsCapsule(*m_state, capsule, cylinderHeight))
        {
            return false;
        }

        capsule.resize(cylinderHeight);
        capsule.invalidateCache();

        return true;
    }

    std::optional<float> PhysicsWorld::controllerHeight(ControllerId controller) const
    {
        const ControllerSlot* slot = m_state->findController(controller);

        if (slot == nullptr)
        {
            return std::nullopt;
        }

        return slot->controller->getHeight() + 2.0f * slot->controller->getRadius();
    }

    bool PhysicsWorld::isGrounded(ControllerId controller) const noexcept
    {
        const ControllerSlot* slot = m_state->findController(controller);
        return slot != nullptr && slot->isGrounded;
    }
} //namespace lockstep::physics
