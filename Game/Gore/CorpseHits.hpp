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

    // The local player's new shots this frame against dead bodies: each shot is traced against
    // the ragdolls drawn on this machine (only up to whatever the gameplay ray hits first: level,
    // living characters). A hit pushes the body part at the hit point and bleeds right away, and
    // is reported to the host (queueCorpseHit()), which decides on a sever and tells both machines
    // (the other one then pushes its own copy of the body). Presentation only; host NPC bullets
    // never hit bodies.
    // state:     the game state.
    // engine:    Engine services.
    // eye:       the camera position the shots leave from.
    // direction: unit aim direction.
    // canFire:   the local player's shots are bullets now (armed, weapon up, not carrying a toy).
    void presentCorpseShots(lockstep::app::GameState& state, lockstep::app::EngineContext& engine,
        const math::Vec3& eye, const math::Vec3& direction, bool canFire);
} //namespace game

#endif //LOCKSTEP_GAME_GORE_CORPSE_HITS_HPP
