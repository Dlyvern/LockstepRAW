#include "Level/Arena.hpp"

#include <Lockstep/Physics/PhysicsWorld.hpp>

#include <cmath>

namespace game
{
    namespace
    {
        namespace physics = lockstep::physics;

        constexpr math::Vec3 k_tiltAxis{1.0f, 0.0f, 0.0f};
        constexpr float k_floorThickness{0.2f};
        constexpr float k_wallThickness{0.5f};
        constexpr float k_wallHeight{3.0f};
        constexpr float k_stepHeight{0.25f};
        constexpr float k_platformHeight{0.75f};
        constexpr float k_rampThickness{0.2f};
        constexpr float k_rampOverlap{0.1f};

        ArenaBox wall(float x, float z, float halfX, float halfZ)
        {
            return ArenaBox{.center = {x, k_wallHeight * 0.5f, z}, .halfExtents = {halfX, k_wallHeight * 0.5f, halfZ}};
        }

        ArenaBox block(float x, float z, float halfX, float height, float halfZ)
        {
            return ArenaBox{.center = {x, height * 0.5f, z}, .halfExtents = {halfX, height * 0.5f, halfZ}};
        }

        ArenaBox ramp(float x, float halfX, float lowZ, float highZ, float height)
        {
            const float run = lowZ - highZ;
            const float angle = std::atan2(height, run);
            const float halfLength = std::sqrt(run * run + height * height) * 0.5f + k_rampOverlap;
            const float halfThickness = k_rampThickness * 0.5f;
            const math::Vec3 topMiddle{x, height * 0.5f, (lowZ + highZ) * 0.5f};
            const math::Vec3 normal{0.0f, std::cos(angle), std::sin(angle)};

            return ArenaBox{.center = topMiddle - normal * halfThickness,
                .halfExtents = {halfX, halfThickness, halfLength},
                .tilt = angle};
        }
    } //namespace

    Arena buildArena()
    {
        const float wallOffset = k_arenaHalfExtent + k_wallThickness * 0.5f;
        const float wallSpan = k_arenaHalfExtent + k_wallThickness;

        Arena arena;
        arena.boxes = {
            ArenaBox{.center = {0.0f, -k_floorThickness * 0.5f, 0.0f},
                .halfExtents = {wallSpan, k_floorThickness * 0.5f, wallSpan},
                .surface = ArenaSurface::Floor},
            wall(0.0f, -wallOffset, wallSpan, k_wallThickness * 0.5f),
            wall(0.0f, wallOffset, wallSpan, k_wallThickness * 0.5f),
            wall(-wallOffset, 0.0f, k_wallThickness * 0.5f, wallSpan),
            wall(wallOffset, 0.0f, k_wallThickness * 0.5f, wallSpan),
            block(-2.25f, 5.0f, 1.75f, k_stepHeight, 0.5f),
            block(-2.25f, 4.0f, 1.75f, k_stepHeight * 2.0f, 0.5f),
            block(0.0f, 2.75f, 4.0f, k_platformHeight, 0.75f),
            ramp(2.25f, 1.75f, 6.5f, 3.5f, k_platformHeight),
            block(-1.0f, 0.0f, 1.5f, 2.0f, 0.3f),
            block(6.0f, -5.0f, 0.4f, 2.0f, 1.5f),
            block(-7.0f, -8.0f, 1.5f, 1.2f, 0.4f)};

        return arena;
    }

    lockstep::Status createArenaBodies(Arena& arena, physics::PhysicsWorld& physics)
    {
        arena.bodies.clear();

        for (const ArenaBox& box : arena.boxes)
        {
            auto body = physics.createBody({
                .shape = physics::BoxShape{.halfExtents = box.halfExtents},
                .type = physics::BodyType::Static,
                .transform = {.position = box.center, .rotation = math::angleAxis(box.tilt, k_tiltAxis)}});

            if (!body)
            {
                return body.error();
            }

            arena.bodies.push_back(body.value());
        }

        return {};
    }

    math::Transform arenaBoxTransform(const ArenaBox& box)
    {
        return math::Transform{.position = box.center,
            .rotation = math::angleAxis(box.tilt, k_tiltAxis),
            .scale = box.halfExtents * 2.0f};
    }
} //namespace game
