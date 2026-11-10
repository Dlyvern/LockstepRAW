#ifndef LOCKSTEP_GAME_SHELL_SHELL_SCREENS_HPP
#define LOCKSTEP_GAME_SHELL_SHELL_SCREENS_HPP

#include "Shell/Shell.hpp"

#include <Lockstep/UI/UiTypes.hpp>

#include <string_view>

namespace lockstep::app
{
    struct EngineContext;
    struct GameState;
} //namespace lockstep::app

namespace game
{
    // Height of one menu row and the gap between rows, in canvas units.
    inline constexpr float k_menuRowHeight{50.0f};
    inline constexpr float k_menuRowGap{6.0f};

    // Where a screen's title and column go: the left edge, the title's top and the column's area.
    // screenColumn() picks it for the canvas.
    struct ScreenLayout
    {
        math::Vec2 title{0.0f, 0.0f};
        lockstep::ui::Rect column{};
    };

    // Lays out a screen: a title top-left and a column of rows under it.
    // ui:     the UI context (for the canvas size).
    // width:  the column width, in canvas units.
    // Returns: the layout.
    [[nodiscard]] ScreenLayout screenLayout(const lockstep::ui::UiContext& ui, float width);

    // Draws the main menu: Host, Join, Options, Credits, Quit.
    // state:  the game state.
    // engine: Engine services.
    void drawMainScreen(lockstep::app::GameState& state, lockstep::app::EngineContext& engine);

    // Draws the host screen (name, port, Host) and opens the lobby when hosting starts.
    // state:  the game state.
    // engine: Engine services.
    void drawHostScreen(lockstep::app::GameState& state, lockstep::app::EngineContext& engine);

    // Draws the join screen (name, address, port, Connect) and opens the lobby when connecting
    // starts.
    // state:  the game state.
    // engine: Engine services.
    void drawJoinScreen(lockstep::app::GameState& state, lockstep::app::EngineContext& engine);

    // Draws the lobby: both seats, the level, Ready, Start (host) and Leave.
    // state:  the game state.
    // engine: Engine services.
    void drawLobbyScreen(lockstep::app::GameState& state, lockstep::app::EngineContext& engine);

    // Draws the scrolling credits.
    // state:        the game state.
    // engine:       Engine services.
    // frameSeconds: real seconds since the last frame (the scroll).
    void drawCreditsScreen(lockstep::app::GameState& state, lockstep::app::EngineContext& engine, float frameSeconds);

    // Draws the pause menu: Resume, Options, Quit to main menu, Quit game.
    // state:  the game state.
    // engine: Engine services.
    void drawPauseScreen(lockstep::app::GameState& state, lockstep::app::EngineContext& engine);

    // Draws the options screen with its tabs; changes apply and save at once.
    // state:        the game state.
    // engine:       Engine services.
    // frameSeconds: real seconds since the last frame (the notice line).
    void drawOptionsScreen(lockstep::app::GameState& state, lockstep::app::EngineContext& engine, float frameSeconds);

    // Opens the options screen, remembering where Back returns to.
    // shell:    the shell.
    // engine:   Engine services (the current graphics quality).
    // returnTo: Main or Pause.
    void openOptions(Shell& shell, lockstep::app::EngineContext& engine, ShellScreen returnTo);

    // Closes the pause menu and takes the mouse back for the game.
    // shell:  the shell.
    // engine: Engine services.
    void resumeGame(Shell& shell, lockstep::app::EngineContext& engine);
} //namespace game

#endif //LOCKSTEP_GAME_SHELL_SHELL_SCREENS_HPP
