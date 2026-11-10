#ifndef LOCKSTEP_PHYSICS_RAGDOLL_STATE_HPP
#define LOCKSTEP_PHYSICS_RAGDOLL_STATE_HPP

#include "Lockstep/Core/Transform.hpp"

#include <PxPhysicsAPI.h>

#include <cstdint>
#include <vector>

namespace lockstep::physics
{
    // One body of a living ragdoll.
    // actor:     its PhysX body, or nullptr once removed.
    // joint:     the joint to its parent, or nullptr for the root or a detached body.
    // parent:    index of the parent body, or k_noRagdollBody.
    // previous:  its world frame before the last step.
    // current:   its world frame after the last step.
    struct RagdollPart
    {
        physx::PxRigidDynamic* actor{nullptr};
        physx::PxD6Joint* joint{nullptr};
        uint32_t parent{0};
        math::Transform previous{};
        math::Transform current{};
    };

    // A living ragdoll: its bodies and the aggregate that groups them in the broad phase.
    struct RagdollSlot
    {
        physx::PxAggregate* aggregate{nullptr};
        std::vector<RagdollPart> parts;
    };

    // Releases every joint and body of a ragdoll, then its aggregate.
    // slot: the ragdoll; left empty.
    void releaseRagdoll(RagdollSlot& slot) noexcept;

    // Copies each simulated body's current frame into its previous frame. Called before a step.
    // slot: the ragdoll.
    void beginRagdollStep(RagdollSlot& slot) noexcept;

    // Reads each simulated body's frame after a step into its current frame.
    // slot: the ragdoll.
    void endRagdollStep(RagdollSlot& slot) noexcept;

    // Filter data marking a ragdoll body's shape.
    // ragdollIndex:  the ragdoll's slot index (bodies of the same ragdoll share it).
    // body:          the body's index in the ragdoll.
    // ignoredBodies: bit mask of bodies of the same ragdoll it never collides with.
    // selfCollision: whether it collides with bodies of its own ragdoll at all.
    // Returns:       the shape's simulation filter data.
    [[nodiscard]] physx::PxFilterData ragdollFilterData(uint32_t ragdollIndex, uint32_t body, uint32_t ignoredBodies,
        bool selfCollision) noexcept;

    // The scene's simulation filter shader. Pairs without a ragdoll shape behave as with
    // PxDefaultSimulationFilterShader. A ragdoll shape touches static bodies, other ragdolls and
    // the bodies of its own ragdoll its filter data allows; nothing else.
    physx::PxFilterFlags ragdollAwareFilterShader(physx::PxFilterObjectAttributes attributes0,
        physx::PxFilterData filterData0, physx::PxFilterObjectAttributes attributes1, physx::PxFilterData filterData1,
        physx::PxPairFlags& pairFlags, const void* constantBlock, physx::PxU32 constantBlockSize);
} //namespace lockstep::physics

#endif //LOCKSTEP_PHYSICS_RAGDOLL_STATE_HPP
