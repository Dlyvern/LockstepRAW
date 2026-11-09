#ifndef LOCKSTEP_GAME_PLAYER_PLAYER_COMMAND_HPP
#define LOCKSTEP_GAME_PLAYER_PLAYER_COMMAND_HPP

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
    struct PlayerCommand
    {
        math::Vec2 move{0.0f, 0.0f};
        float yaw{0.0f};
        float pitch{0.0f};
        uint8_t shotCount{0};
        bool bulletTime{false};
        uint8_t interactCount{0};
    };

    // The local player's look, shot and interact counters, kept between ticks to build
    // PlayerCommands.
    // yaw, pitch:    accumulated mouse look, in radians.
    // shotCount:     shots fired so far, wrapping at 256.
    // interactCount: interact presses so far, wrapping at 256.
    struct LocalControls
    {
        float yaw{0.0f};
        float pitch{0.0f};
        uint8_t shotCount{0};
        uint8_t interactCount{0};
    };

    // Settings for turning device input into commands.
    // mouseSensitivity: radians of look per pixel of mouse movement.
    // maxPitch:         largest look angle up or down, in radians.
    struct ControlSettings
    {
        float mouseSensitivity{0.0025f};
        float maxPitch{1.45f};
    };

    // Builds this tick's command from local input: WASD to move, mouse to look, left mouse to
    // fire, E to interact, Space for bullet-time. Look, fire and interact only count while the
    // cursor is captured.
    // controls:        the local look and shot counter, updated in place.
    // settings:        sensitivity and pitch limit.
    // input:           this tick's input.
    // isCursorCaptured: true while the window holds the mouse for mouse look.
    // Returns:         the command.
    [[nodiscard]] PlayerCommand sampleLocalCommand(LocalControls& controls, const ControlSettings& settings,
        const lockstep::platform::Input& input, bool isCursorCaptured);

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
