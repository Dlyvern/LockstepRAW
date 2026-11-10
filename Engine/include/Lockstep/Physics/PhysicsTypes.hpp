#ifndef LOCKSTEP_PHYSICS_PHYSICS_TYPES_HPP
#define LOCKSTEP_PHYSICS_PHYSICS_TYPES_HPP

#include "Lockstep/Core/Math.hpp"
#include "Lockstep/Core/Transform.hpp"

#include <cstdint>
#include <variant>
#include <vector>

namespace lockstep::physics
{
    // Stable handle to a rigid body. Same rules as world::EntityId: the generation detects a
    // handle kept after its body was destroyed, and a default-constructed handle is null.
    struct BodyId
    {
        uint32_t index{0};
        uint32_t generation{0};

        // Returns: true for the null handle. A non-null handle may still be stale.
        [[nodiscard]] constexpr bool isNull() const noexcept
        {
            return generation == 0;
        }

        friend constexpr bool operator==(BodyId, BodyId) noexcept = default;
    };

    // Stable handle to a character controller. Same rules as BodyId.
    struct ControllerId
    {
        uint32_t index{0};
        uint32_t generation{0};

        // Returns: true for the null handle. A non-null handle may still be stale.
        [[nodiscard]] constexpr bool isNull() const noexcept
        {
            return generation == 0;
        }

        friend constexpr bool operator==(ControllerId, ControllerId) noexcept = default;
    };

    // How a body takes part in the simulation.
    // Static:  never moves on its own (level geometry). setTransform() still teleports it.
    // Dynamic: moved by the simulation: gravity, contacts, velocities.
    enum class BodyType : uint8_t
    {
        Static,
        Dynamic
    };

    // Axis-aligned box in the body's local space. halfExtents: half the size on each axis.
    struct BoxShape
    {
        math::Vec3 halfExtents{0.5f, 0.5f, 0.5f};
    };

    // Sphere centred on the body. radius: in metres.
    struct SphereShape
    {
        float radius{0.5f};
    };

    // Capsule centred on the body, along its local Y axis.
    // radius:     radius of the cylinder and the two end caps.
    // halfHeight: half the length of the cylinder part (without the caps).
    struct CapsuleShape
    {
        float radius{0.5f};
        float halfHeight{0.5f};
    };

    // Infinite plane: the points p with dot(normal, p) == distance; the solid side is below.
    // Static bodies only; the body transform is ignored.
    struct PlaneShape
    {
        math::Vec3 normal{0.0f, 1.0f, 0.0f};
        float distance{0.0f};
    };

    // Convex hull of a point cloud in the body's local space, computed when the body is created
    // (PhysX keeps at most 32 hull vertices). For small dynamic pieces such as a severed limb.
    // points: at least 4 points that are not all in one plane.
    struct ConvexShape
    {
        std::vector<math::Vec3> points;
    };

    // The collision shape of a body.
    using BodyShape = std::variant<BoxShape, SphereShape, CapsuleShape, PlaneShape, ConvexShape>;

    // Settings for a new rigid body.
    // shape:       its collision shape.
    // type:        static or dynamic.
    // transform:   where it starts. Scale is ignored; size comes from the shape.
    // density:     kg per cubic metre, gives a dynamic body its mass. Ignored for static bodies.
    // friction:    friction coefficient for both static and dynamic friction.
    // restitution: bounciness, 0 (none) to 1.
    // isDebris:    presentation-only debris each machine simulates for itself (gibs, casings):
    //              raycasts and character controllers pass through it, so it never changes
    //              game rules; it still collides with the level and other bodies.
    struct BodyDescription
    {
        BodyShape shape{BoxShape{}};
        BodyType type{BodyType::Static};
        math::Transform transform{};
        float density{1000.0f};
        float friction{0.5f};
        float restitution{0.0f};
        bool isDebris{false};
    };

    // Settings for a new character controller: an upright capsule moved by sweeps, not by the
    // simulation.
    // footPosition:      bottom of the capsule.
    // radius:            capsule radius.
    // height:            total height, feet to top of the head. Must be more than 2 * radius.
    // stepOffset:        tallest step the character walks up without jumping.
    // slopeLimitDegrees: steepest slope it can walk up; steeper slopes block and slide it.
    // contactOffset:     skin around the capsule that keeps it from touching geometry exactly.
    struct ControllerDescription
    {
        math::Vec3 footPosition{0.0f, 0.0f, 0.0f};
        float radius{0.4f};
        float height{1.8f};
        float stepOffset{0.35f};
        float slopeLimitDegrees{45.0f};
        float contactOffset{0.02f};
    };

    // How PhysicsWorld::setControllerHeight() treats a controller that grows.
    // MustFit: growing fails (nothing changes) when the taller capsule would overlap static or
    //          kinematic geometry, e.g. standing up under a table.
    // Force:   resize anyway, e.g. to match a size the host already checked.
    enum class ControllerResize : uint8_t
    {
        MustFit,
        Force
    };

    // What a controller hit during one move().
    // grounded:   it touched ground below: standing, can jump, should not fall.
    // hitCeiling: it touched something above.
    // hitSides:   it touched something to the side.
    struct ControllerMove
    {
        bool grounded{false};
        bool hitCeiling{false};
        bool hitSides{false};
    };

    // What a raycast should not hit. Null handles ignore nothing.
    // body:       a body to pass through, e.g. the shooter's own collider.
    // controller: a controller to pass through, e.g. the shooter's pawn.
    struct RaycastIgnore
    {
        BodyId body{};
        ControllerId controller{};
    };

    // The closest thing a raycast hit. Exactly one of `body` and `controller` is non-null.
    // body:       the body hit.
    // controller: the character controller hit.
    // point:      world-space hit point.
    // normal:     surface normal at the hit point.
    // distance:   distance from the ray origin.
    struct RaycastHit
    {
        BodyId body{};
        ControllerId controller{};
        math::Vec3 point{0.0f, 0.0f, 0.0f};
        math::Vec3 normal{0.0f, 1.0f, 0.0f};
        float distance{0.0f};
    };

    // Counters for the dev tools.
    // bodyCount:            living rigid bodies.
    // controllerCount:      living character controllers.
    // ragdollCount:         living ragdolls.
    // ragdollBodyCount:     simulated bodies of all ragdolls (not counted in bodyCount).
    // lastStepSeconds:      simulated seconds of the last step() (0 while the world is frozen).
    // lastSubsteps:         PhysX simulate calls made by the last step().
    // lastStepMilliseconds: wall time the last step() took.
    // errorCount:           PhysX errors reported so far.
    // warningCount:         PhysX warnings reported so far.
    struct PhysicsStats
    {
        uint32_t bodyCount{0};
        uint32_t controllerCount{0};
        uint32_t ragdollCount{0};
        uint32_t ragdollBodyCount{0};
        float lastStepSeconds{0.0f};
        uint32_t lastSubsteps{0};
        float lastStepMilliseconds{0.0f};
        uint32_t errorCount{0};
        uint32_t warningCount{0};
    };
} //namespace lockstep::physics

#endif //LOCKSTEP_PHYSICS_PHYSICS_TYPES_HPP
