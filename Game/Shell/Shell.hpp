#ifndef LOCKSTEP_GAME_SHELL_SHELL_HPP
#define LOCKSTEP_GAME_SHELL_SHELL_HPP

#include "Shell/Credits.hpp"
#include "Shell/Lobby.hpp"
#include "Shell/MenuBackdrop.hpp"
#include "Shell/MenuStyle.hpp"
#include "Shell/UserSettings.hpp"

#include <Lockstep/Render/GraphicsQuality.hpp>
#include <Lockstep/UI/UiContext.hpp>

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace lockstep::app
{
    struct EngineContext;
    struct FrameInfo;
    struct GameState;
    struct TickInfo;
} //namespace lockstep::app

namespace game
{
    // The screens of the game shell.
    // None:    no menu (playing).
    // Main:    Host, Join, Options, Credits, Quit.
    // Host:    name and port before hosting.
    // Join:    name, address and port before connecting.
    // Lobby:   both players, ready states, the level and Start.
    // Options: graphics, audio, controls, accessibility.
    // Credits: the scrolling credits.
    // Pause:   the in-game pause menu (the game keeps running).
    enum class ShellScreen : uint8_t
    {
        None,
        Main,
        Host,
        Join,
        Lobby,
        Options,
        Credits,
        Pause
    };

    // The tabs of the options screen.
    enum class OptionsTab : uint8_t
    {
        Graphics,
        Audio,
        Controls,
        Accessibility
    };

    // What the options screen edits.
    // tab:        the open tab.
    // quality:    the graphics quality being edited (applied live).
    // resolution: index into the window size list.
    // notice:     a short line about what was saved or needs a restart.
    // noticeSeconds: real seconds the notice stays.
    // returnTo:   the screen Back goes to (Main or Pause).
    struct OptionsState
    {
        OptionsTab tab{OptionsTab::Graphics};
        lockstep::render::GraphicsQuality quality{};
        std::size_t resolution{0};
        std::string notice;
        float noticeSeconds{0.0f};
        ShellScreen returnTo{ShellScreen::Main};
    };

    // Dev automation of the shell (dev builds; `--dev-menu`, `--dev-auto-ready`, `--dev-name`,
    // `--dev-leave`).
    // openScreen:   the screen to show first ("main", "host", "join", "options", "options:audio",
    //               "options:controls", "options:accessibility", "credits", "lobby", "pause").
    // isAutoReady:  in the lobby, press Ready after a moment; the host then starts as soon as the
    //               partner is ready too.
    // lobbySeconds: real seconds spent in the lobby.
    // isOpened:     openScreen was applied.
    // leaveSeconds: `--dev-leave <seconds>`: in game, quit to the main menu after that long.
    struct ShellDev
    {
        std::string openScreen;
        bool isAutoReady{false};
        float lobbySeconds{0.0f};
        bool isOpened{false};
        float leaveSeconds{0.0f};
    };

    // The game shell: main menu with its live background, host / join, the lobby, options,
    // credits and the in-game pause menu. Presentation and session setup only; the game itself
    // starts through GameSession when the lobby starts.
    // isInMenu:      no game runs: the menu background, the menus and the lobby.
    // screen:        the screen shown.
    // nextScreen:    the screen being faded to (None when no change is under way).
    // fade:          the shown screen's opacity, 0 to 1.
    // isPauseOpen:   in game: the pause menu (or options from it) is open.
    // ui:            the immediate-mode UI.
    // style:         the body-cam look's textures and clock.
    // settings:      the player's options.
    // settingsFile:  the settings file.
    // options:       the options screen's state.
    // lobby:         the pre-game lobby.
    // backdrop:      the main menu's 3D background.
    // credits:       the credits lines (loaded when first shown).
    // creditsSeconds: real seconds the credits have scrolled.
    // nameText, portText, addressText: the host / join fields.
    // notice:        an error or note shown on the host, join or main screen.
    // isEndRequested: quit to the main menu at the next tick.
    // menuSeconds:   real seconds since the menu was shown (the fade from black).
    // gameAmbience:  the level's ambience loop while a game runs.
    // isClientGame:  the running game was joined as a client (losing the host ends it).
    // dev:           dev automation.
    struct Shell
    {
        bool isInMenu{false};
        ShellScreen screen{ShellScreen::None};
        ShellScreen nextScreen{ShellScreen::None};
        float fade{0.0f};
        bool isPauseOpen{false};
        lockstep::ui::UiContext ui{};
        MenuStyle style{};
        UserSettings settings{};
        std::filesystem::path settingsFile{};
        OptionsState options{};
        Lobby lobby{};
        MenuBackdrop backdrop{};
        std::vector<CreditLine> credits{};
        float creditsSeconds{0.0f};
        std::string nameText;
        std::string portText;
        std::string addressText;
        std::string notice;
        bool isEndRequested{false};
        float menuSeconds{0.0f};
        std::optional<lockstep::audio::VoiceHandle> gameAmbience{};
        bool isClientGame{false};
        ShellDev dev{};
    };

    // Creates the shell at startup: reads the options and applies them, creates the menu look.
    // state:  the game state (state.shell is filled).
    // engine: Engine services.
    void initShell(lockstep::app::GameState& state, lockstep::app::EngineContext& engine);

    // Opens the main menu instead of a game: loads the background and, when `--host` or
    // `--connect` already started the session, opens the lobby.
    // state:  the game state.
    // engine: Engine services.
    void enterMenu(lockstep::app::GameState& state, lockstep::app::EngineContext& engine);

    // Runs the shell's part of a fixed tick: in the menu the lobby (and starting the game from
    // it); in game a requested quit to the menu, or a lost connection.
    // state:  the game state.
    // engine: Engine services.
    // tick:   the tick.
    // Returns: true if the game's own tick must not run (menu, or the game just ended).
    bool tickShell(lockstep::app::GameState& state, lockstep::app::EngineContext& engine,
        const lockstep::app::TickInfo& tick);

    // Presents a frame: in the menu the background and menus; in game the game (presentFrame())
    // with the pause menu over it (Escape opens and closes it).
    // state:     the game state.
    // engine:    Engine services.
    // frameInfo: this frame's input and timing.
    void presentShell(lockstep::app::GameState& state, lockstep::app::EngineContext& engine,
        const lockstep::app::FrameInfo& frameInfo);

    // shell:   the shell.
    // Returns: true while a menu takes the player's input (the game sees idle input).
    [[nodiscard]] bool isShellBlockingInput(const Shell& shell);

    // Starts fading to another screen.
    // shell:  the shell.
    // screen: the screen to show.
    void goToScreen(Shell& shell, ShellScreen screen);
} //namespace game

#endif //LOCKSTEP_GAME_SHELL_SHELL_HPP
