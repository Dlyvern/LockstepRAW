#ifndef LOCKSTEP_GAME_SHELL_SHELL_DEV_TOOLS_HPP
#define LOCKSTEP_GAME_SHELL_SHELL_DEV_TOOLS_HPP

#if LOCKSTEP_DEV_TOOLS
namespace lockstep::app
{
    struct EngineContext;
    struct GameState;
} //namespace lockstep::app

namespace game
{
    struct Shell;

    // Opens the screen `--dev-menu` asks for, once, as soon as it can be shown (the pause menu
    // once a game runs, the others in the main menu).
    // state:  the game state.
    // engine: Engine services.
    void applyDevScreen(lockstep::app::GameState& state, lockstep::app::EngineContext& engine);

    // `--dev-auto-ready`: presses Ready shortly after the lobby opens, and on the host starts as
    // soon as a partner is present and ready.
    // shell:       the shell.
    // engine:      Engine services.
    // tickSeconds: the tick length.
    void tickDevLobby(Shell& shell, lockstep::app::EngineContext& engine, float tickSeconds);
} //namespace game
#endif

#endif //LOCKSTEP_GAME_SHELL_SHELL_DEV_TOOLS_HPP
