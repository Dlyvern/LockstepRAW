#ifndef LOCKSTEP_PHYSICS_PHYSICS_WORLD_HPP
#define LOCKSTEP_PHYSICS_PHYSICS_WORLD_HPP

#include "Lockstep/Core/Result.hpp"
#include "Lockstep/Physics/PhysicsTypes.hpp"

#include <cstddef>
#include <memory>
#include <optional>
#include <span>
#include <vector>

namespace lockstep::physics
{
    struct PhysicsState;

    // Longest PhysX simulate call. step() splits a longer delta into equal substeps.
    inline constexpr float k_maxSubstepSeconds{1.0f / 60.0f};

    // Most substeps one step() makes; a longer delta is simulated as this many max-length steps.
    inline constexpr uint32_t k_maxSubsteps{8};

    // Settings for PhysicsWorld::create().
    // gravity:       acceleration on dynamic bodies, in m/s^2. Controllers get none; Game adds
    //                gravity to their displacement.
    // workerThreads: PhysX simulation worker threads.
    struct PhysicsDescription
    {
        math::Vec3 gravity{0.0f, -9.81f, 0.0f};
        uint32_t workerThreads{2};
    };

    // The physics scene: rigid bodies, raycasts and character controllers, on PhysX. Owned by
    // the run loop and reachable through app::EngineContext.
    //
    // Timing: the run loop calls step() once per fixed tick with the World time-domain delta,
    // before GameApi::fixedTick. So in fixedTick, body transforms already show this tick, and
    // velocities or transforms Game sets apply from the next tick's step. Character controllers
    // do not depend on step(): move() sweeps the capsule through the scene immediately, so Game
    // moves pawns with the Player delta and a client can replay its own pawn's moves for
    // prediction.
    //
    // Bodies are not entities. Game keeps a BodyId next to its EntityId and copies transform()
    // into the entity in fixedTick.
    //
    // Only one PhysicsWorld may exist at a time (PhysX allows one foundation per process).
    // Nothing here throws; PhysX errors are counted in stats() and printed to stderr.
    class PhysicsWorld
    {
    public:
        // Creates the PhysX foundation, physics, scene, default material and controller manager.
        // description: gravity and threading.
        // Returns:     the world, or an error if PhysX could not be initialised (also when
        //              another PhysicsWorld exists).
        [[nodiscard]] static Result<PhysicsWorld> create(const PhysicsDescription& description = {});

        // Releases every body, controller and the PhysX scene, then PhysX itself.
        ~PhysicsWorld();

        // Takes over another world. `other` becomes empty and may only be destroyed.
        PhysicsWorld(PhysicsWorld&& other) noexcept;

        // Destroys this world, then takes over `other`. `other` becomes empty.
        PhysicsWorld& operator=(PhysicsWorld&& other) noexcept;

        PhysicsWorld(const PhysicsWorld&) = delete;
        PhysicsWorld& operator=(const PhysicsWorld&) = delete;

        // Advances the rigid-body simulation. Called by the run loop; Game does not call it.
        // deltaSeconds: simulated time, the World domain's delta for this tick. Zero or less
        //               leaves the world frozen. Split into equal substeps of at most
        //               k_maxSubstepSeconds.
        void step(float deltaSeconds);

        // Creates a rigid body.
        // description: shape, type, start transform and material.
        // Returns:     its handle, or InvalidArgument for a bad shape (non-positive size, a
        //              dynamic plane), or an error PhysX reported.
        [[nodiscard]] Result<BodyId> createBody(const BodyDescription& description);

        // Creates a static triangle-mesh body (level geometry), cooking the mesh with PhysX.
        // Triangles collide from both sides with controllers and raycasts. Large triangles are
        // accepted without PhysX's size warning (level floors and walls are meant to be large).
        // positions: mesh-space vertex positions.
        // indices:   triangle list into `positions`; a multiple of 3, every index in range.
        // transform: world placement. Scale may be non-uniform; zero scale is rejected.
        // Returns:   its handle, or InvalidArgument for bad data, or an error if cooking failed
        //            (e.g. every triangle was degenerate).
        [[nodiscard]] Result<BodyId> createStaticMesh(std::span<const math::Vec3> positions,
            std::span<const uint32_t> indices, const math::Transform& transform);

        // Creates static triangle-mesh bodies from a mesh cooked offline by MeshCooker, without
        // cooking at runtime. All the bodies share the one loaded mesh; it lives as long as any of
        // them. Collision behaves exactly as with createStaticMesh().
        // cooked:     the bytes MeshCooker::cook() produced (same PhysX version).
        // transforms: world placement of each body. Scale may be non-uniform; zero is rejected.
        // Returns:    one handle per transform, in order, or InvalidArgument for a zero scale, or
        //             PhysicsFailure if the data is not a cooked mesh PhysX can load (nothing is
        //             created then).
        [[nodiscard]] Result<std::vector<BodyId>> createStaticMeshes(std::span<const std::byte> cooked,
            std::span<const math::Transform> transforms);

        // Destroys a body. Its handle, and every copy, becomes stale.
        // body:    the body.
        // Returns: true if it was alive and is now destroyed.
        bool destroyBody(BodyId body);

        // body:    the body.
        // Returns: true if `body` refers to a living body.
        [[nodiscard]] bool isAlive(BodyId body) const noexcept;

        // body:    the body.
        // Returns: its position and rotation (scale 1), or nothing for a stale handle.
        [[nodiscard]] std::optional<math::Transform> transform(BodyId body) const;

        // Teleports a body. Dynamic bodies keep their velocity and are woken up.
        // body:      the body.
        // transform: new position and rotation; scale is ignored.
        // Returns:   false for a stale handle.
        bool setTransform(BodyId body, const math::Transform& transform);

        // body:    the body.
        // Returns: its linear velocity in m/s (zero for a static body), or nothing for a stale
        //          handle.
        [[nodiscard]] std::optional<math::Vec3> linearVelocity(BodyId body) const;

        // body:    the body.
        // Returns: its angular velocity in rad/s (zero for a static body), or nothing for a
        //          stale handle.
        [[nodiscard]] std::optional<math::Vec3> angularVelocity(BodyId body) const;

        // Sets a dynamic body's velocities and wakes it up.
        // body:     the body.
        // linear:   linear velocity in m/s.
        // angular:  angular velocity in rad/s.
        // Returns:  false for a stale handle or a static body.
        bool setVelocity(BodyId body, const math::Vec3& linear, const math::Vec3& angular = {});

        // Finds the closest body or controller along a ray. Sees the scene as of the last step(),
        // plus controllers where their last move() left them.
        // origin:      ray start in world space.
        // direction:   ray direction; need not be normalised. A zero direction hits nothing.
        // maxDistance: longest hit distance.
        // ignore:      a body and/or controller the ray passes through.
        // Returns:     the closest hit, or nothing.
        [[nodiscard]] std::optional<RaycastHit> raycast(const math::Vec3& origin, const math::Vec3& direction,
            float maxDistance, const RaycastIgnore& ignore = {}) const;

        // Creates a character controller (an upright capsule).
        // description: start position, size, step offset and slope limit.
        // Returns:     its handle, or InvalidArgument for a bad size, or an error PhysX reported.
        [[nodiscard]] Result<ControllerId> createController(const ControllerDescription& description);

        // Destroys a controller. Its handle, and every copy, becomes stale.
        // controller: the controller.
        // Returns:    true if it was alive and is now destroyed.
        bool destroyController(ControllerId controller);

        // controller: the controller.
        // Returns:    true if `controller` refers to a living controller.
        [[nodiscard]] bool isAlive(ControllerId controller) const noexcept;

        // Moves a controller by sweeping its capsule: it slides along walls, walks up steps
        // up to the step offset, is blocked by slopes over the slope limit, static bodies and
        // other controllers. Dynamic (simulated) bodies are ignored: they may exist on one
        // machine only (host-side toys), and a pawn's move must give the same result on the
        // host and on a predicting client. The capsule still pushes dynamic bodies away during
        // step(). Independent of step(), so it can be replayed.
        // controller:   the controller.
        // displacement: desired movement this tick, gravity included (velocity * Player delta).
        // deltaSeconds: the Player domain's delta for this tick.
        // Returns:      what it touched, or nothing for a stale handle.
        std::optional<ControllerMove> move(ControllerId controller, const math::Vec3& displacement, float deltaSeconds);

        // controller: the controller.
        // Returns:    the bottom of its capsule, or nothing for a stale handle.
        [[nodiscard]] std::optional<math::Vec3> footPosition(ControllerId controller) const;

        // Teleports a controller without collision, e.g. to a spawn or to a host-corrected
        // position before replaying moves.
        // controller: the controller.
        // position:   new bottom of the capsule.
        // Returns:    false for a stale handle.
        bool setFootPosition(ControllerId controller, const math::Vec3& position);

        // Changes a controller's total height (feet to top of the head), keeping its feet where
        // they are, e.g. to crouch. Only static and kinematic bodies can block a grow; simulated
        // bodies and other controllers never do (as in move(), so host and client agree).
        // controller: the controller.
        // height:     the new total height; must be more than twice its radius.
        // resize:     whether a grow must fit (MustFit) or happens anyway (Force).
        // Returns:    true if the controller now has `height`; false for a stale handle, an
        //             invalid height or a grow that did not fit.
        bool setControllerHeight(ControllerId controller, float height,
            ControllerResize resize = ControllerResize::MustFit);

        // controller: the controller.
        // Returns:    its total height (feet to top of the head), or nothing for a stale handle.
        [[nodiscard]] std::optional<float> controllerHeight(ControllerId controller) const;

        // controller: the controller.
        // Returns:    whether its last move() ended on the ground (false before the first move or
        //             for a stale handle).
        [[nodiscard]] bool isGrounded(ControllerId controller) const noexcept;

        // Returns: body and controller counts, last step timing and PhysX error counts.
        [[nodiscard]] PhysicsStats stats() const noexcept;

    private:
        explicit PhysicsWorld(std::unique_ptr<PhysicsState> state) noexcept;

        std::unique_ptr<PhysicsState> m_state;
    };
} //namespace lockstep::physics

#endif //LOCKSTEP_PHYSICS_PHYSICS_WORLD_HPP
