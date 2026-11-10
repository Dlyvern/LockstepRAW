#include "Shell/ShellDevTools.hpp"

#include "GameState.hpp"
#include "Shell/ShellScreens.hpp"

#include <Lockstep/Platform/Window.hpp>

#include <array>
#include <iostream>
#include <string_view>
#include <utility>

#if LOCKSTEP_DEV_TOOLS
namespace game
{
    namespace
    {
        using lockstep::app::EngineContext;
        using lockstep::app::GameState;

        constexpr float k_autoReadySeconds{1.5f};
    } //namespace

    void applyDevScreen(GameState& state, EngineContext& engine)
    {
        Shell& shell = state.shell;

        if (shell.dev.isOpened || shell.dev.openScreen.empty())
        {
            return;
        }

        const std::string_view wanted = shell.dev.openScreen;
        const bool isPause = wanted == "pause";

        if (isPause == shell.isInMenu)
        {
            return;
        }

        shell.dev.isOpened = true;

        if (isPause)
        {
            shell.isPauseOpen = true;
            engine.window.setCursorCaptured(false);
            shell.screen = ShellScreen::Pause;
        }
        else if (wanted.starts_with("options"))
        {
            openOptions(shell, engine, ShellScreen::Main);
            const std::array<std::pair<std::string_view, OptionsTab>, 3> tabs{{{"options:audio", OptionsTab::Audio},
                {"options:controls", OptionsTab::Controls}, {"options:accessibility", OptionsTab::Accessibility}}};

            for (const auto& [name, tab] : tabs)
            {
                shell.options.tab = name == wanted ? tab : shell.options.tab;
            }
        }
        else if (wanted == "host")
        {
            goToScreen(shell, ShellScreen::Host);
        }
        else if (wanted == "join")
        {
            goToScreen(shell, ShellScreen::Join);
        }
        else if (wanted == "credits")
        {
            goToScreen(shell, ShellScreen::Credits);
        }
        else if (wanted == "lobby")
        {
            if (auto opened = openHostLobby(shell.lobby, engine, shell.nameText, shell.settings.port); opened)
            {
                goToScreen(shell, ShellScreen::Lobby);
            }
        }

        if (shell.nextScreen != ShellScreen::None)
        {
            shell.screen = std::exchange(shell.nextScreen, ShellScreen::None);
            shell.fade = 1.0f;
        }
    }

    void tickDevLobby(Shell& shell, EngineContext& engine, float tickSeconds)
    {
        Lobby& lobby = shell.lobby;

        if (!shell.dev.isAutoReady || !lobby.isOpen)
        {
            return;
        }

        shell.dev.lobbySeconds += tickSeconds;
        LobbyPlayer& local = lobby.players[lobby.isHost ? 0 : 1];

        if (!local.isReady && shell.dev.lobbySeconds > k_autoReadySeconds && !lobby.isFailed)
        {
            std::clog << "Game: dev auto-ready\n";
            setLobbyReady(lobby, engine, true);
        }

        if (lobby.isHost && lobby.players[1].isPresent && canStartLobby(lobby, engine) && !lobby.startLevel)
        {
            std::clog << "Game: dev auto-start\n";
            startLobby(lobby, engine);
        }
    }
} //namespace game
#endif
