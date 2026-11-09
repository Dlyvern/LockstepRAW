#ifndef LOCKSTEP_GAME_LEVEL_ARENA_HPP
#define LOCKSTEP_GAME_LEVEL_ARENA_HPP

#include <Lockstep/Core/Math.hpp>
#include <Lockstep/Core/Result.hpp>
#include <Lockstep/Core/Transform.hpp>
#include <Lockstep/Physics/PhysicsTypes.hpp>

#include <cstdint>
#include <vector>

namespace lockstep::physics
{
    class PhysicsWorld;
} //namespace lockstep::physics

namespace game
{
    namespace math = lockstep::math;

    // Distance from the arena centre to the inside of each wall along x and z, in metres.
    inline constexpr float k_arenaHalfExtent{19.0f};

    // How an arena box is drawn. Floor: the checkerboard. Solid: walls, cover, steps and ramps.
    enum class ArenaSurface : uint8_t
    {
        Floor,
        Solid
    };

    // One static box of the arena, the stand-in level geometry until levels come from Blender.
    // center:      box centre in world space.
    // halfExtents: half the box size along its local axes.
    // tilt:        rotation around the x axis, in radians (ramps).
    // surface:     how it is drawn.
    struct ArenaBox
    {
        math::Vec3 center{0.0f, 0.0f, 0.0f};
        math::Vec3 halfExtents{0.5f, 0.5f, 0.5f};
        float tilt{0.0f};
        ArenaSurface surface{ArenaSurface::Solid};
    };

    // The arena: its boxes, and on the host the static bodies made from them.
    // boxes:  the layout, the same on host and client.
    // bodies: one static body per box (host only), in the same order.
    struct Arena
    {
        std::vector<ArenaBox> boxes;
        std::vector<lockstep::physics::BodyId> bodies;
    };

    // Returns: the arena layout: floor, four walls, cover boxes, two steps and a ramp up to a
    //          platform.
    [[nodiscard]] Arena buildArena();

    // Host: creates one static box body per arena box.
    // arena:   the arena; its bodies are filled in.
    // physics: the physics world.
    // Returns: success, or the error of the first body that could not be created.
    [[nodiscard]] lockstep::Status createArenaBodies(Arena& arena, lockstep::physics::PhysicsWorld& physics);

    // box:     an arena box.
    // Returns: its transform, with the scale set to the full box size (for drawing a unit cube).
    [[nodiscard]] math::Transform arenaBoxTransform(const ArenaBox& box);
} //namespace game

#endif //LOCKSTEP_GAME_LEVEL_ARENA_HPP
