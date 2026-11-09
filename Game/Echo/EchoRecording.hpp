#ifndef LOCKSTEP_GAME_ECHO_ECHO_RECORDING_HPP
#define LOCKSTEP_GAME_ECHO_ECHO_RECORDING_HPP

#include "Level/RoomState.hpp"
#include "Player/Pawns.hpp"

#include <Lockstep/Core/Math.hpp>
#include <Lockstep/Core/Result.hpp>
#include <Lockstep/Core/Transform.hpp>

#include <array>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <vector>

namespace game
{
    // Where the host saves the recording unless `--echo-file <path>` says otherwise, relative to
    // the working directory.
    inline constexpr const char* k_defaultEchoPath{"Saved/echo_recording.bin"};

    // One pawn on one tick of a recording.
    // isPresent: the pawn existed (the player had joined and was alive).
    // foot:      its controller's foot position.
    // yaw:       look angle around the up axis, radians (0 looks along -z).
    // pitch:     look angle up or down, radians.
    struct EchoPawnSample
    {
        bool isPresent{false};
        math::Vec3 foot{0.0f, 0.0f, 0.0f};
        float yaw{0.0f};
        float pitch{0.0f};
    };

    // Both pawns on one tick.
    struct EchoFrame
    {
        std::array<EchoPawnSample, k_playerCount> pawns{};
    };

    // An interaction that happened during the recording.
    // frame:  index into EchoRecording::frames.
    // player: who interacted.
    // object: what with.
    // action: what it did (Refused when the rules said no).
    struct EchoEvent
    {
        uint32_t frame{0};
        uint8_t player{0};
        RoomObject object{RoomObject::Other};
        RoomAction action{RoomAction::Inspect};
    };

    // The "Echo" recording: both players' pawns and interactions at the fixed tick rate, from
    // level start until the front door opens. The epilogue replays it as ghosts.
    //
    // File (little-endian): "LECH", uint32 version (1), uint32 tickRate, uint32 frameCount,
    // uint32 eventCount, then frameCount frames of k_playerCount samples (uint8 isPresent,
    // float footX, footY, footZ, yaw, pitch), then eventCount events (uint32 frame, uint8 player,
    // uint8 object, uint8 action).
    // tickRate: frames per second.
    // frames:   one per fixed tick.
    // events:   interactions, oldest first.
    struct EchoRecording
    {
        uint32_t tickRate{60};
        std::vector<EchoFrame> frames;
        std::vector<EchoEvent> events;
    };

    // Writes a recording, creating the folder if needed.
    // recording: the recording.
    // path:      the file.
    // Returns:   success, or an error if the file can't be written.
    [[nodiscard]] lockstep::Status saveEchoRecording(const EchoRecording& recording, const std::filesystem::path& path);

    // Reads a recording written by saveEchoRecording().
    // path:    the file.
    // Returns: the recording, or an error if it can't be read or is not a valid recording.
    [[nodiscard]] lockstep::Result<EchoRecording> loadEchoRecording(const std::filesystem::path& path);

    // A recording played back as ghost pawns. Client-only presentation: never replicated, so each
    // machine can play its own.
    // recording: what plays.
    // frame:     the frame shown on the newest tick.
    // nextEvent: index of the next event to announce.
    struct EchoPlayback
    {
        EchoRecording recording;
        std::size_t frame{0};
        std::size_t nextEvent{0};
    };

    // Advances playback by one fixed tick, looping at the end, and logs the interactions it
    // passes.
    // playback: the playback.
    void stepEchoPlayback(EchoPlayback& playback);

    // Where each ghost pawn is shown between the last two ticks.
    // playback: the playback.
    // alpha:    interpolation between the previous and the current tick, 0 to 1.
    // Returns:  for each player, the pawn transform (centre, turned to its yaw), or nothing where
    //           the pawn was absent.
    [[nodiscard]] std::array<std::optional<math::Transform>, k_playerCount> echoGhostTransforms(
        const EchoPlayback& playback, float alpha);

    // The Echo command-line flags.
    // savePath: `--echo-file <path>`: where the host saves the recording (k_defaultEchoPath
    //           without the flag).
    // playPath: `--echo-play <path>`: a recording to play back as ghosts from the start.
    struct EchoLaunch
    {
        std::filesystem::path savePath{k_defaultEchoPath};
        std::optional<std::filesystem::path> playPath;
    };

    // Reads the Echo flags; see EchoLaunch.
    // arguments: command-line options.
    // Returns:   the flags, or an error for a flag without a value.
    [[nodiscard]] lockstep::Result<EchoLaunch> parseEchoLaunch(std::span<const char* const> arguments);
} //namespace game

#endif //LOCKSTEP_GAME_ECHO_ECHO_RECORDING_HPP
