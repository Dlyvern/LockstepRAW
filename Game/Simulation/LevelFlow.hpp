#ifndef LOCKSTEP_GAME_SIMULATION_LEVEL_FLOW_HPP
#define LOCKSTEP_GAME_SIMULATION_LEVEL_FLOW_HPP

#include "Level/SceneLevel.hpp"

#include <Lockstep/App/GameApi.hpp>
#include <Lockstep/Core/Result.hpp>

#include <cstddef>
#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace game
{
    // The level the district morning leads to when no `--next-level` is given.
    inline constexpr std::string_view k_defaultNextLevel{"levels/station/scene"};

    // Which level is loaded and where the story goes next. When the district morning ends (the
    // drive has faded to black), the host loads the next level from the mounted content, tells the
    // client with a reliable message, and both fade in at its PlayerStart_1/2.
    // launched:        the level both machines were started with (`--level` or `--scene`).
    // current:         the level loaded now (virtual path or scene file).
    // next:            host: the level after the morning (`--next-level`, default
    //                  k_defaultNextLevel); empty after a failed switch.
    // isClientInformed: host: the connected client was told about `current`.
    // enteredSeconds:  Player seconds since a switch loaded `current`, for the fade-in.
    // isEntering:      the fade-in after a switch is running.
    // isAutoplay:      dev (`--dev-autoplay`): the host finishes the morning and advances the
    //                  station by itself, for tests without a keyboard.
    // autoplaySeconds: Player seconds since the last autoplay step.
    // isSwitchRequested: host: a mission asked to go to `next` now (requestLevelSwitch()).
    struct LevelFlow
    {
        std::string launched;
        std::string current;
        std::string next;
        bool isClientInformed{false};
        float enteredSeconds{0.0f};
        bool isEntering{false};
        bool isAutoplay{false};
        float autoplaySeconds{0.0f};
        bool isSwitchRequested{false};
    };

    // Host: asks for a switch to another level at the end of this tick (e.g. the hospital's
    // service lift), like the end of the morning's drive.
    // flow:  the level flow.
    // level: the level's virtual path; it must be mounted.
    void requestLevelSwitch(LevelFlow& flow, std::string_view level);

    // Loads the start level (or builds the arena without one) and sets it up: weapon zones, level
    // characters, spawns, dialogue, NPC placeholders and the station mission or the apartment
    // room. Reads `--next-level <virtual path>` and, in dev builds, `--dev-autoplay`.
    // state:   the game state.
    // engine:  Engine services.
    // launch:  the level from the command line, or nothing for the arena.
    // Returns: success, or a load, Echo or `--next-level` error.
    [[nodiscard]] lockstep::Status startLevel(lockstep::app::GameState& state, lockstep::app::EngineContext& engine,
        const std::optional<SceneLaunch>& launch);

    // Host, end of a tick: tells a newly connected client which level is loaded (a client started
    // in another level, or joining after a switch, loads it), switches to the
    // next level once the morning's drive has faded out, and runs the dev autoplay.
    // state:  the game state.
    // engine: Engine services.
    void updateLevelFlowAsHost(lockstep::app::GameState& state, lockstep::app::EngineContext& engine);

    // Client: takes a Game message if it is a level switch, and loads that level (dropping the
    // predicted pawn; it comes back at the new spawn with the host's next pawn state).
    // state:   the game state.
    // engine:  Engine services.
    // message: a received Game message.
    // Returns: true if it was a level message.
    bool receiveLevelMessage(lockstep::app::GameState& state, lockstep::app::EngineContext& engine,
        std::span<const std::byte> message);

    // Both machines, end of a tick: NPC lines near the local player, subtitles onto the HUD and
    // the fade-in after a switch.
    // state:  the game state.
    // engine: Engine services.
    void presentLevel(lockstep::app::GameState& state, lockstep::app::EngineContext& engine);
} //namespace game

#endif //LOCKSTEP_GAME_SIMULATION_LEVEL_FLOW_HPP
