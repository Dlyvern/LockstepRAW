#ifndef LOCKSTEP_PHYSICS_RAGDOLL_HPP
#define LOCKSTEP_PHYSICS_RAGDOLL_HPP

#include "Lockstep/Core/Math.hpp"
#include "Lockstep/Core/Transform.hpp"

#include <cstdint>
#include <vector>

namespace lockstep::physics
{
    // Stable handle to a ragdoll. Same rules as BodyId: the generation detects a handle kept
    // after its ragdoll was destroyed, and a default-constructed handle is null.
    struct RagdollId
    {
        uint32_t index{0};
        uint32_t generation{0};

        // Returns: true for the null handle. A non-null handle may still be stale.
        [[nodiscard]] constexpr bool isNull() const noexcept
        {
            return generation == 0;
        }

        friend constexpr bool operator==(RagdollId, RagdollId) noexcept = default;
    };

    // RagdollBodyDescription::parent of the root body.
    inline constexpr uint32_t k_noRagdollBody{0xFFFFFFFFu};

    // Most bodies one ragdoll may have.
    inline constexpr uint32_t k_maxRagdollBodies{32};

    // How far a body's joint may turn away from the rest pose, in radians. The joint frame's
    // twist axis runs along the bone; "bend" turns the bone towards its bend direction (negative
    // values turn it away), "side" turns it across. Every angle must be within (-pi, pi).
    // twist:   half the range of rotation around the bone.
    // bendMin: furthest turn away from the bend direction (usually <= 0).
    // bendMax: furthest turn towards the bend direction.
    // side:    half the range of sideways turning.
    struct RagdollJointLimits
    {
        float twist{0.5f};
        float bendMin{-0.5f};
        float bendMax{0.5f};
        float side{0.5f};
    };

    // One body of a ragdoll: a capsule rigidly attached to one bone, joined to its parent body at
    // the bone's origin. Rest-space values may use any common space (e.g. the model space of
    // the bind pose); they only define shapes and joint frames relative to each other.
    // parent:        index of the parent body (lower than this body's), or k_noRagdollBody.
    // rest:          the bone's frame (origin and rotation; scale ignored) in the rest pose.
    // pose:          the bone's world frame now; where the body starts.
    // capsuleStart:  one end of the capsule's axis, rest space.
    // capsuleEnd:    the other end; may equal capsuleStart for a sphere.
    // radius:        capsule radius in metres.
    // twistAxis:     rest-space direction along the bone (the joint's twist axis).
    // bendDirection: rest-space direction the joint bends towards (e.g. forward for a hip, back
    //                for a knee); need not be exactly perpendicular to twistAxis.
    // limits:        joint limits relative to the rest pose. Unused for a root.
    // density:       kg per cubic metre.
    struct RagdollBodyDescription
    {
        uint32_t parent{k_noRagdollBody};
        math::Transform rest{};
        math::Transform pose{};
        math::Vec3 capsuleStart{0.0f, 0.0f, 0.0f};
        math::Vec3 capsuleEnd{0.0f, 0.0f, 0.0f};
        float radius{0.05f};
        math::Vec3 twistAxis{0.0f, 1.0f, 0.0f};
        math::Vec3 bendDirection{0.0f, 0.0f, 1.0f};
        RagdollJointLimits limits{};
        float density{1000.0f};
    };

    // Material and solver settings shared by every body of a ragdoll.
    // friction:            friction coefficient against the level.
    // restitution:         bounciness, 0 to 1.
    // linearDamping:       slows bodies down over time (air drag), per second.
    // angularDamping:      slows their spinning, per second.
    // jointDamping:        resists relative rotation at every joint (muscle tone); 0 is limp.
    // positionIterations:  solver position iterations per body (more is stiffer and costlier).
    // velocityIterations:  solver velocity iterations per body.
    // sleepThreshold:      kinetic energy per unit mass below which bodies may fall asleep.
    // maxDepenetration:    fastest speed, m/s, at which overlaps are pushed apart.
    // selfCollision:       bodies of one ragdoll collide with each other, except neighbours
    //                      (parent, children, siblings) and pairs already touching at creation.
    struct RagdollSettings
    {
        float friction{0.8f};
        float restitution{0.0f};
        float linearDamping{0.05f};
        float angularDamping{0.6f};
        float jointDamping{2.0f};
        uint32_t positionIterations{12};
        uint32_t velocityIterations{2};
        float sleepThreshold{0.02f};
        float maxDepenetration{2.0f};
        bool selfCollision{true};
    };

    // Everything PhysicsWorld::createRagdoll() needs.
    // bodies:          the bodies, parents before children; the first is the root.
    // linearVelocity:  starting velocity of the whole ragdoll, m/s.
    // angularVelocity: starting spin of the whole ragdoll around the root body, rad/s.
    // settings:        material and solver settings.
    struct RagdollDescription
    {
        std::vector<RagdollBodyDescription> bodies;
        math::Vec3 linearVelocity{0.0f, 0.0f, 0.0f};
        math::Vec3 angularVelocity{0.0f, 0.0f, 0.0f};
        RagdollSettings settings{};
    };

    // How a ragdoll is moving, for settle detection.
    // isAsleep:       every simulated body is asleep.
    // maxSpeed:       fastest linear speed of any simulated body, m/s.
    // simulatedCount: bodies still simulated (not removed).
    struct RagdollStatus
    {
        bool isAsleep{false};
        float maxSpeed{0.0f};
        uint32_t simulatedCount{0};
    };
} //namespace lockstep::physics

#endif //LOCKSTEP_PHYSICS_RAGDOLL_HPP
