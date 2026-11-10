#ifndef LOCKSTEP_GAME_PRESENTATION_WEAPON_PRESENTATION_HPP
#define LOCKSTEP_GAME_PRESENTATION_WEAPON_PRESENTATION_HPP

#include "Presentation/HeadMotion.hpp"

#include <Lockstep/App/GameApi.hpp>
#include <Lockstep/Core/Math.hpp>
#include <Lockstep/Core/Transform.hpp>

#include <cstdint>
#include <optional>

namespace game
{
    namespace math = lockstep::math;

    // The weapon handling of one presented frame, after presentViewModel(): takes the sight
    // offset the next command sends (GameState::Local::aimOffset; the drawn front post aiming
    // down sights, the look from the hip), submits both players' flashlights (the local one from
    // the drawn weapon's rail, the partner's from its right hand along its replicated pitch) and
    // plays the flashlight, fire mode and held-breath sounds. Call before the level's lights.
    // state:         the game state.
    // engine:        Engine services.
    // localPawn:     the local pawn's drawn transform, or nothing.
    // head:          the camera pose of this frame (field of view offset).
    // playerSeconds: Player time since the previous frame.
    void presentWeaponHandling(lockstep::app::GameState& state, lockstep::app::EngineContext& engine,
        const std::optional<math::Transform>& localPawn, const HeadPose& head, float playerSeconds);

    // The direction of one of the local player's shots, as the host traces it (shotDirection() of
    // the newest command).
    // state:      the game state.
    // engine:     Engine services.
    // shotNumber: the shot counter value of the shot.
    // Returns:    unit direction.
    [[nodiscard]] math::Vec3 localShotDirection(const lockstep::app::GameState& state,
        const lockstep::app::EngineContext& engine, uint8_t shotNumber);

    // state:   the game state.
    // Returns: depth of field strength the held breath asks for (presentLensEffects()).
    [[nodiscard]] float heldBreathFocus(const lockstep::app::GameState& state);

#if LOCKSTEP_DEV_TOOLS
    // Draws the Weapons section of the Game dev panel: fire rate, camera recoil, spread,
    // penetration, flashlight and the handling sounds.
    // state: the game state.
    void weaponsDevSection(lockstep::app::GameState& state);
#endif
} //namespace game

#endif //LOCKSTEP_GAME_PRESENTATION_WEAPON_PRESENTATION_HPP
