#ifndef LOCKSTEP_GAME_GORE_CORPSE_HITS_HPP
#define LOCKSTEP_GAME_GORE_CORPSE_HITS_HPP

#include <Lockstep/App/GameApi.hpp>
#include <Lockstep/Core/Math.hpp>

#include <cstdint>

namespace game
{
    namespace math = lockstep::math;

    // The local player's shots as the corpse tracer last saw them.
    // lastShots: the local shot counter last seen.
    // hasLast:   lastShots is valid (false until the first frame).
    struct CorpseShots
    {
        uint8_t lastShots{0};
        bool hasLast{false};
    };

    // The local player's new shots this frame against dead bodies and loose pieces: each shot is
    // traced against the ragdoll bodies (frozen ones too), severed pieces and dropped weapons on
    // this machine, only up to whatever the gameplay ray hits first (level, living characters).
    // A hit pushes what it struck at the hit point and bleeds right away; hits on bodies and
    // pieces are reported to the host (queueCorpseHit()), which decides on a sever and tells both
    // machines (the other one then pushes its own copy). Presentation only; the impulses act in
    // World time, so they show in bullet-time too.
    // state:     the game state.
    // engine:    Engine services.
    // eye:       the camera position the shots leave from.
    // direction: unit aim direction.
    // canFire:   the local player's shots are bullets now (armed, weapon up, not carrying a toy).
    void presentCorpseShots(lockstep::app::GameState& state, lockstep::app::EngineContext& engine,
        const math::Vec3& eye, const math::Vec3& direction, bool canFire);

    // Fires one of the local player's bullets at the bodies and pieces, as presentCorpseShots()
    // does for each new shot (dev drills call it directly).
    // state:     the game state.
    // engine:    Engine services.
    // eye:       where the bullet starts.
    // direction: unit direction it flies.
    void fireAtBodies(lockstep::app::GameState& state, lockstep::app::EngineContext& engine, const math::Vec3& eye,
        const math::Vec3& direction);

    // Host, after the NPCs have fired this tick: traces their shots against the dead bodies and
    // severed pieces drawn on the host and queues the hits like a player's (applied next tick, so
    // both machines push and bleed).
    // state:  the game state.
    // engine: Engine services.
    void shootBodiesWithNpcShots(lockstep::app::GameState& state, lockstep::app::EngineContext& engine);
} //namespace game

#endif //LOCKSTEP_GAME_GORE_CORPSE_HITS_HPP
