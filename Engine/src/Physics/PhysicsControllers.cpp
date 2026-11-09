#include "Lockstep/Physics/PhysicsWorld.hpp"

#include "PhysXConversions.hpp"
#include "PhysicsState.hpp"

#include <cmath>

namespace lockstep::physics
{
    namespace
    {
        constexpr float k_minMoveDistance{1.0e-4f};
        const physx::PxVec3 k_upDirection{0.0f, 1.0f, 0.0f};

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

        const physx::PxControllerFilters filters;
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

    bool PhysicsWorld::isGrounded(ControllerId controller) const noexcept
    {
        const ControllerSlot* slot = m_state->findController(controller);
        return slot != nullptr && slot->isGrounded;
    }
} //namespace lockstep::physics
