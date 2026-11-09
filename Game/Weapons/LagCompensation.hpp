#ifndef LOCKSTEP_GAME_WEAPONS_LAG_COMPENSATION_HPP
#define LOCKSTEP_GAME_WEAPONS_LAG_COMPENSATION_HPP

#include <Lockstep/Core/Math.hpp>
#include <Lockstep/Physics/PhysicsTypes.hpp>

#include <cstdint>
#include <deque>
#include <span>
#include <vector>

namespace lockstep::physics
{
    class PhysicsWorld;
} //namespace lockstep::physics

namespace game
{
    namespace math = lockstep::math;

    // Farthest back a shot is checked, in ticks (250 ms at 60 Hz). A client further behind than
    // this has its shots checked against the world 250 ms ago.
    inline constexpr uint64_t k_maxRewindTicks{24};

    // Where one controller stood.
    // controller: the controller.
    // foot:       its foot position.
    struct RewindPose
    {
        lockstep::physics::ControllerId controller{};
        math::Vec3 foot{0.0f, 0.0f, 0.0f};
    };

    // Where the hittable controllers stood after one host tick.
    // tick:  the host tick.
    // poses: one per controller.
    struct LagFrame
    {
        uint64_t tick{0};
        std::vector<RewindPose> poses;
    };

    // The host's record of the last k_maxRewindTicks ticks of hittable controllers (pawns and
    // the enemy), oldest first, so a client's shot can be checked against what the client saw.
    struct LagHistory
    {
        std::deque<LagFrame> frames;
    };

    // A rewind in effect: what to put back, and how far back it went.
    // restore: the controllers moved and where they stood before.
    // ticks:   how many ticks back from the newest frame the world was moved.
    struct Rewind
    {
        std::vector<RewindPose> restore;
        double ticks{0.0};
    };

    // Host: records where the controllers stand after this tick and forgets frames older than
    // k_maxRewindTicks. Stale or null controllers are left out.
    // history:     the history.
    // physics:     the physics world.
    // tick:        this host tick.
    // controllers: the hittable controllers.
    void recordLagFrame(LagHistory& history, const lockstep::physics::PhysicsWorld& physics, uint64_t tick,
        std::span<const lockstep::physics::ControllerId> controllers);

    // Host: moves the recorded controllers, except the shooter, to where they stood at
    // `seenTick` (blending the two frames around it). Controllers no longer alive are skipped.
    // Undo with restoreRewind() before anything else moves.
    // history:     the history.
    // physics:     the physics world.
    // seenTick:    the host tick (fractional) the shooter saw; clamped to the history, at most
    //              k_maxRewindTicks back.
    // shooter:     the shooter's controller, left where it is.
    // Returns:     what to restore and how far back it went.
    [[nodiscard]] Rewind rewindTo(const LagHistory& history, lockstep::physics::PhysicsWorld& physics, double seenTick,
        lockstep::physics::ControllerId shooter);

    // Host: puts the controllers moved by rewindTo() back.
    // physics: the physics world.
    // rewind:  the rewind to undo.
    void restoreRewind(lockstep::physics::PhysicsWorld& physics, const Rewind& rewind);
} //namespace game

#endif //LOCKSTEP_GAME_WEAPONS_LAG_COMPENSATION_HPP
