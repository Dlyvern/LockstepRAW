#ifndef LOCKSTEP_GAME_PRESENTATION_FIRST_PERSON_HPP
#define LOCKSTEP_GAME_PRESENTATION_FIRST_PERSON_HPP

#include "Presentation/HeadMotion.hpp"

#include <Lockstep/App/GameApi.hpp>
#include <Lockstep/Core/Math.hpp>
#include <Lockstep/Core/Transform.hpp>

#include <optional>

namespace game
{
    // The local player's look as the camera shows it this frame: the newest command's look plus
    // the raw mouse movement of frames since that tick (applied with the same sensitivity), so the
    // view turns every frame instead of at the tick rate. Presentation only; shots and the host
    // use the command's look.
    // yaw, pitch: radians, as in PlayerCommand.
    struct FirstPersonLook
    {
        float yaw{0.0f};
        float pitch{0.0f};
    };

    // Accumulates this frame's mouse movement (dropped once a tick has taken it into a command)
    // and returns the look to show.
    // state:     the game state (GameState::Local::pendingLook is updated).
    // engine:    Engine services (cursor capture).
    // frameInfo: this frame's input and tick count.
    // Returns:   the look to show.
    [[nodiscard]] FirstPersonLook presentedLook(lockstep::app::GameState& state,
        const lockstep::app::EngineContext& engine, const lockstep::app::FrameInfo& frameInfo);

    // state:   the game state.
    // engine:  Engine services.
    // Returns: true while the local pawn runs (sprint held, moving forward faster than walking).
    [[nodiscard]] bool isLocalSprinting(const lockstep::app::GameState& state,
        const lockstep::app::EngineContext& engine);

    // Advances the local camera's head motion from the local pawn's predicted (client) or
    // simulated (host) movement, lean and shots.
    // state:         the game state.
    // engine:        Engine services.
    // localPawn:     the local pawn's drawn transform.
    // look:          the look shown this frame.
    // playerSeconds: Player time since the previous frame.
    // Returns:       the camera pose.
    [[nodiscard]] HeadPose presentHead(lockstep::app::GameState& state, const lockstep::app::EngineContext& engine,
        const math::Transform& localPawn, const FirstPersonLook& look, float playerSeconds);

    // Draws the first-person weapon of an armed local player, or hides it. While a reload runs
    // the viewmodel plays its reload clip (tactical or empty, stretched to the reload time), or
    // dips into the lowered pose when the model has no reload clips.
    // state:         the game state.
    // engine:        Engine services.
    // eye:           the camera position, or nothing without a local pawn.
    // head:          the camera pose of this frame.
    // look:          the look shown this frame.
    // playerSeconds: Player time since the previous frame.
    void presentViewModel(lockstep::app::GameState& state, lockstep::app::EngineContext& engine,
        const std::optional<math::Vec3>& eye, const HeadPose& head, const FirstPersonLook& look, float playerSeconds);
} //namespace game

#endif //LOCKSTEP_GAME_PRESENTATION_FIRST_PERSON_HPP
