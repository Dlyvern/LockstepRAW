#ifndef LOCKSTEP_GAME_WEAPONS_HITSCAN_HPP
#define LOCKSTEP_GAME_WEAPONS_HITSCAN_HPP

#include <Lockstep/Core/Math.hpp>
#include <Lockstep/Physics/PhysicsTypes.hpp>

#include <cstdint>

namespace lockstep::physics
{
    class PhysicsWorld;
} //namespace lockstep::physics

namespace game
{
    namespace math = lockstep::math;

    // What a shot struck first.
    // Miss:   nothing in range.
    // Enemy:  the enemy.
    // Player: another player's pawn (no friendly fire; it just stops the shot).
    // Level:  level geometry such as a wall or cover.
    enum class ShotOutcome : uint8_t
    {
        Miss,
        Enemy,
        Player,
        Level
    };

    // The result of tracing one shot.
    // outcome:    what it struck.
    // distance:   distance to the struck thing; 0 on a miss.
    // controller: the controller struck, for Enemy and Player.
    // body:       the body struck, for Level.
    struct ShotResult
    {
        ShotOutcome outcome{ShotOutcome::Miss};
        float distance{0.0f};
        lockstep::physics::ControllerId controller{};
        lockstep::physics::BodyId body{};
    };

    // Hitscan weapon settings.
    // range: farthest a shot reaches, in metres.
    struct HitscanSettings
    {
        float range{100.0f};
    };

    // Traces an instant shot through the physics scene. The first thing on the ray counts, so
    // walls, cover and other players block it.
    // physics:   the physics world.
    // origin:    the shooter's eyes.
    // direction: unit look direction.
    // range:     farthest a shot reaches.
    // shooter:   the shooter's controller, which the ray passes through.
    // enemy:     the enemy's controller, or null while it is dead.
    // Returns:   what the shot struck.
    [[nodiscard]] ShotResult traceShot(const lockstep::physics::PhysicsWorld& physics, const math::Vec3& origin,
        const math::Vec3& direction, float range, lockstep::physics::ControllerId shooter,
        lockstep::physics::ControllerId enemy);

    // Returns: a readable name for `outcome`, for logs and the dev panel.
    [[nodiscard]] const char* toString(ShotOutcome outcome) noexcept;
} //namespace game

#endif //LOCKSTEP_GAME_WEAPONS_HITSCAN_HPP
