#ifndef LOCKSTEP_GAME_PLAYER_PLAYER_COMMAND_HPP
#define LOCKSTEP_GAME_PLAYER_PLAYER_COMMAND_HPP

#include "Weapons/WeaponStance.hpp"

#include <Lockstep/Core/Math.hpp>
#include <Lockstep/Platform/Input.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

namespace game
{
    namespace math = lockstep::math;

    // Bytes an encoded PlayerCommand takes.
    inline constexpr std::size_t k_playerCommandBytes{13};

    // What one player wants to do this tick. The client sends it to the host every tick; the host
    // builds its own player's from local input. The host simulates both players from these.
    // move:       wanted movement in the pawn's frame, x to the right and y forward, each in [-1, 1].
    // yaw:        look direction around the up axis, in radians. 0 looks along -z.
    // pitch:      look direction up (positive) or down, in radians.
    // shotCount:  shots fired so far, wrapping at 256. The host fires once per increase, so a lost
    //             or repeated message neither drops nor doubles a shot (up to 255 at a time).
    // bulletTime: true while the player holds bullet-time.
    // interactCount: interact presses (E) so far, wrapping at 256; counted like `shotCount`.
    // sprint:     true while the player holds run (Shift); only speeds up forward moves.
    // aim:        true while the armed player aims down sights (right mouse button, not while the
    //             weapon must stay lowered); no running then.
    // lower:      the player's lower toggle (X): wants the weapon lowered where that is allowed.
    // isWeaponUp: the player is armed with the weapon raised; caps running at
    //             PawnSettings::weaponUpRunSpeed. The client sets it to predict its own pawn; the
    //             host overwrites it from its own armed and lowered state.
    struct PlayerCommand
    {
        math::Vec2 move{0.0f, 0.0f};
        float yaw{0.0f};
        float pitch{0.0f};
        uint8_t shotCount{0};
        bool bulletTime{false};
        uint8_t interactCount{0};
        bool sprint{false};
        bool aim{false};
        bool lower{false};
        bool isWeaponUp{false};
    };

    // The local player's look, shot and interact counters, kept between ticks to build
    // PlayerCommands.
    // yaw, pitch:    accumulated mouse look, in radians.
    // shotCount:     shots fired so far, wrapping at 256.
    // interactCount: interact presses so far, wrapping at 256.
    // wantsLowered:  the lower toggle; fire or aim clears it to raise the weapon.
    struct LocalControls
    {
        float yaw{0.0f};
        float pitch{0.0f};
        uint8_t shotCount{0};
        uint8_t interactCount{0};
        bool wantsLowered{false};
    };

    // The key that lowers or raises the weapon.
    inline constexpr lockstep::platform::Key k_lowerWeaponKey{lockstep::platform::Key::X};

    // What decides how fire, aim and the lower key act this tick, as the local machine predicts it.
    // isArmed:      the player carries a weapon (as last replicated).
    // zone:         the weapon zone the local pawn stands in.
    // isHoldingToy: the player carries a toy; a click then throws it, even with the weapon lowered.
    struct WeaponControl
    {
        bool isArmed{false};
        WeaponZone zone{WeaponZone::Open};
        bool isHoldingToy{false};
    };

    // Settings for turning device input into commands.
    // mouseSensitivity: radians of look per pixel of mouse movement.
    // maxPitch:         largest look angle up or down, in radians.
    struct ControlSettings
    {
        float mouseSensitivity{0.0025f};
        float maxPitch{1.45f};
    };

    // Builds this tick's command from local input: WASD to move, Shift to run, mouse to look,
    // left mouse to fire, right mouse to aim down sights, X (k_lowerWeaponKey) to lower or raise
    // the weapon, E to interact, Space for bullet-time. Look, fire, aim, lower and interact only
    // count while the cursor is captured. With the weapon lowered, a click or aim raises it
    // instead of firing (where raising is allowed; a click still throws a held toy).
    // controls:        the local look and shot counter, updated in place.
    // settings:        sensitivity and pitch limit.
    // input:           this tick's input.
    // isCursorCaptured: true while the window holds the mouse for mouse look.
    // weapon:          armed state, zone and held toy.
    // Returns:         the command.
    [[nodiscard]] PlayerCommand sampleLocalCommand(LocalControls& controls, const ControlSettings& settings,
        const lockstep::platform::Input& input, bool isCursorCaptured, const WeaponControl& weapon);

    // Packs a command into the bytes sent to the host.
    // command: the command.
    // Returns: the encoded bytes.
    [[nodiscard]] std::array<std::byte, k_playerCommandBytes> encodeCommand(const PlayerCommand& command);

    // Unpacks a command the client sent.
    // bytes:   the received bytes.
    // Returns: the command, or nothing if `bytes` is not a valid command.
    [[nodiscard]] std::optional<PlayerCommand> decodeCommand(std::span<const std::byte> bytes);
} //namespace game

#endif //LOCKSTEP_GAME_PLAYER_PLAYER_COMMAND_HPP
