#ifndef LOCKSTEP_PHYSICS_PHYSICS_STATE_HPP
#define LOCKSTEP_PHYSICS_PHYSICS_STATE_HPP

#include "Lockstep/Physics/PhysicsTypes.hpp"
#include "Lockstep/Physics/Ragdoll.hpp"

#include "Core/HandlePool.hpp"
#include "PhysXErrorReporter.hpp"
#include "RagdollState.hpp"

#include <PxPhysicsAPI.h>

#include <cstdint>
#include <optional>

namespace lockstep::physics
{
    // What a PhysX actor's userData says it belongs to.
    // Body:       a rigid body; the slot index is in the low 32 bits.
    // Controller: a character controller's kinematic actor; same layout.
    enum class ActorKind : uint8_t
    {
        None,
        Body,
        Controller
    };

    // An actor's owner as decoded from its userData.
    // kind:  what it belongs to.
    // index: slot index in the matching HandlePool.
    struct ActorTag
    {
        ActorKind kind{ActorKind::None};
        uint32_t index{0};
    };

    // Packs an actor's owner into a userData pointer value.
    // kind:    what the actor belongs to.
    // index:   slot index in the matching HandlePool.
    // Returns: the value to store in PxActor::userData.
    [[nodiscard]] void* encodeActorTag(ActorKind kind, uint32_t index) noexcept;

    // Unpacks a userData value written by encodeActorTag().
    // userData: PxActor::userData.
    // Returns:  the owner, or kind None for an untagged actor.
    [[nodiscard]] ActorTag decodeActorTag(const void* userData) noexcept;

    // One character controller and what its last move() reported.
    struct ControllerSlot
    {
        physx::PxCapsuleController* controller{nullptr};
        bool isGrounded{false};
    };

    // Everything behind a PhysicsWorld. The members are released in reverse creation order by
    // the destructor; the allocator and error reporter outlive the foundation that uses them.
    struct PhysicsState
    {
        PhysicsState() = default;

        // Releases controllers, ragdolls, bodies, scene, material, dispatcher, physics and foundation, in
        // that order.
        ~PhysicsState();

        PhysicsState(const PhysicsState&) = delete;
        PhysicsState& operator=(const PhysicsState&) = delete;
        PhysicsState(PhysicsState&&) = delete;
        PhysicsState& operator=(PhysicsState&&) = delete;

        // body:    a public body handle.
        // Returns: the body's actor, or nullptr for a stale handle.
        [[nodiscard]] physx::PxRigidActor* findBody(BodyId body) const noexcept;

        // controller: a public controller handle.
        // Returns:    the controller's slot, or nullptr for a stale handle.
        [[nodiscard]] ControllerSlot* findController(ControllerId controller) noexcept;

        // controller: a public controller handle.
        // Returns:    the controller's slot, or nullptr for a stale handle.
        [[nodiscard]] const ControllerSlot* findController(ControllerId controller) const noexcept;

        // ragdoll: a public ragdoll handle.
        // Returns: the ragdoll's slot, or nullptr for a stale handle.
        [[nodiscard]] RagdollSlot* findRagdoll(RagdollId ragdoll) noexcept;

        // ragdoll: a public ragdoll handle.
        // Returns: the ragdoll's slot, or nullptr for a stale handle.
        [[nodiscard]] const RagdollSlot* findRagdoll(RagdollId ragdoll) const noexcept;

        physx::PxDefaultAllocator allocator;
        PhysXErrorReporter errors;
        physx::PxFoundation* foundation{nullptr};
        physx::PxPhysics* physics{nullptr};
        physx::PxDefaultCpuDispatcher* dispatcher{nullptr};
        physx::PxScene* scene{nullptr};
        physx::PxMaterial* defaultMaterial{nullptr};
        physx::PxControllerManager* controllerManager{nullptr};
        HandlePool<physx::PxRigidActor*> bodies;
        HandlePool<ControllerSlot> controllers;
        HandlePool<RagdollSlot> ragdolls;
        uint32_t ragdollBodyCount{0};
        float lastStepSeconds{0.0f};
        uint32_t lastSubsteps{0};
        float lastStepMilliseconds{0.0f};
    };
} //namespace lockstep::physics

#endif //LOCKSTEP_PHYSICS_PHYSICS_STATE_HPP
