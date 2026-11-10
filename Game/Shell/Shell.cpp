#include "Shell/Shell.hpp"

#include "GameState.hpp"
#include "Presentation/Frame.hpp"
#include "Shell/GameSession.hpp"
#include "Shell/ShellDevTools.hpp"
#include "Shell/ShellScreens.hpp"

#include <Lockstep/Net/Session.hpp>
#include <Lockstep/Platform/Input.hpp>
#include <Lockstep/Platform/Window.hpp>
#include <Lockstep/Render/GraphicsConfig.hpp>
#include <Lockstep/Render/Renderer.hpp>

#include <algorithm>
#include <cstdlib>
#include <array>
#include <iostream>
#include <span>
#include <string>

namespace game
{
    namespace
    {
        namespace net = lockstep::net;
        namespace platform = lockstep::platform;
        namespace ui = lockstep::ui;

        using lockstep::app::EngineContext;
        using lockstep::app::FrameInfo;
        using lockstep::app::GameState;
        using lockstep::app::TickInfo;

        constexpr float k_fadeOutSeconds{0.18f};
        constexpr float k_fadeInSeconds{0.32f};
        constexpr float k_fadeFromBlackSeconds{1.6f};
        constexpr std::string_view k_cameraLabel{"CAM 07  //  UNIT 4-ADAM  //  BODY-WORN"};

        std::optional<std::string_view> flagValue(std::span<const char* const> arguments, std::string_view flag)
        {
            for (std::size_t index = 0; index + 1 < arguments.size(); ++index)
            {
                if (arguments[index] != nullptr && arguments[index + 1] != nullptr
                    && std::string_view(arguments[index]) == flag)
                {
                    return std::string_view(arguments[index + 1]);
                }
            }

            return std::nullopt;
        }

        std::string_view firstWidget(ShellScreen screen)
        {
            switch (screen)
            {
                case ShellScreen::Main:
                {
                    return "main.host";
                }
                case ShellScreen::Host:
                {
                    return "host.start";
                }
                case ShellScreen::Join:
                {
                    return "join.address";
                }
                case ShellScreen::Lobby:
                {
                    return "lobby.ready";
                }
                case ShellScreen::Options:
                {
                    return "opt.tab.0";
                }
                case ShellScreen::Credits:
                {
                    return "credits.back";
                }
                case ShellScreen::Pause:
                {
                    return "pause.resume";
                }
                case ShellScreen::None:
                {
                    break;
                }
            }

            return "";
        }

        void showScreen(Shell& shell, ShellScreen screen)
        {
            shell.screen = screen;
            shell.nextScreen = ShellScreen::None;
            shell.ui.setFocus(firstWidget(screen));
        }

        void returnToMenu(GameState& state, EngineContext& engine, std::string notice)
        {
            Shell& shell = state.shell;
            endGame(state, engine);
            shell.isPauseOpen = false;
            shell.lobby = Lobby{};
            shell.notice = std::move(notice);
            enterMenu(state, engine);
        }

        void beginGame(GameState& state, EngineContext& engine, const std::string& level)
        {
            Shell& shell = state.shell;
            shell.lobby = Lobby{};
            unloadMenuBackdrop(state, engine);
            shell.isInMenu = false;
            shell.screen = ShellScreen::None;
            shell.nextScreen = ShellScreen::None;
            engine.renderer.setScreenFade(0.0f);
            shell.isClientGame = engine.session.role() == net::Role::Client;
            std::optional<SceneLaunch> launch;

            if (!level.empty())
            {
                launch = SceneLaunch{.level = level, .path = {}, .scale = 1.0f};
            }

            std::clog << "Game: starting the game in '" << (level.empty() ? std::string("the arena") : level) << "'\n";

            if (auto started = startGame(state, engine, launch); !started)
            {
                std::clog << "Game: the game did not start: " << started.error().message << '\n';
                returnToMenu(state, engine, "COULD NOT START: " + started.error().message);
                return;
            }

            engine.window.setCursorCaptured(true);
        }

        void stepTransition(Shell& shell, float frameSeconds)
        {
            if (shell.nextScreen == ShellScreen::None)
            {
                shell.fade = std::min(shell.fade + frameSeconds / k_fadeInSeconds, 1.0f);
                return;
            }

            shell.fade -= frameSeconds / k_fadeOutSeconds;

            if (shell.fade <= 0.0f)
            {
                shell.fade = 0.0f;
                showScreen(shell, shell.nextScreen);
            }
        }

        void drawScreen(GameState& state, EngineContext& engine, float frameSeconds)
        {
            switch (state.shell.screen)
            {
                case ShellScreen::Main:
                {
                    drawMainScreen(state, engine);
                    break;
                }
                case ShellScreen::Host:
                {
                    drawHostScreen(state, engine);
                    break;
                }
                case ShellScreen::Join:
                {
                    drawJoinScreen(state, engine);
                    break;
                }
                case ShellScreen::Lobby:
                {
                    drawLobbyScreen(state, engine);
                    break;
                }
                case ShellScreen::Options:
                {
                    drawOptionsScreen(state, engine, frameSeconds);
                    break;
                }
                case ShellScreen::Credits:
                {
                    drawCreditsScreen(state, engine, frameSeconds);
                    break;
                }
                case ShellScreen::Pause:
                {
                    drawPauseScreen(state, engine);
                    break;
                }
                case ShellScreen::None:
                {
                    break;
                }
            }
        }

        void goBack(GameState& state, EngineContext& engine)
        {
            Shell& shell = state.shell;

            switch (shell.screen)
            {
                case ShellScreen::Host:
                case ShellScreen::Join:
                case ShellScreen::Credits:
                {
                    goToScreen(shell, ShellScreen::Main);
                    break;
                }
                case ShellScreen::Lobby:
                {
                    closeLobby(shell.lobby, engine);
                    goToScreen(shell, ShellScreen::Main);
                    break;
                }
                case ShellScreen::Options:
                {
                    goToScreen(shell, shell.options.returnTo);
                    break;
                }
                case ShellScreen::Pause:
                {
                    resumeGame(shell, engine);
                    break;
                }
                case ShellScreen::Main:
                case ShellScreen::None:
                {
                    break;
                }
            }
        }

        void drawMenus(GameState& state, EngineContext& engine, const FrameInfo& frameInfo, bool isOverGame)
        {
            Shell& shell = state.shell;
            ui::UiContext& context = shell.ui;
            stepTransition(shell, frameInfo.frameSeconds);
            context.begin(engine.renderer, frameInfo.input, frameInfo.frameSeconds, shell.nextScreen == ShellScreen::None);
            applyMenuTheme(context);
            const math::Vec2 canvas = context.canvasSize();

            if (isOverGame)
            {
                context.fillRect(ui::rectAt({0.0f, 0.0f}, canvas), math::Vec4{0.0f, 0.0f, 0.0f, 0.55f});
            }
            else if (!shell.backdrop.isLoaded)
            {
                drawMenuGradient(context);
            }

            drawMenuFrame(shell.style, context, frameInfo.frameSeconds, isOverGame ? std::string_view() : k_cameraLabel);
            const float intro = isOverGame ? 1.0f : std::clamp(shell.menuSeconds / 0.8f - 0.3f, 0.0f, 1.0f);
            context.setOpacity(shell.fade * intro);
            drawScreen(state, engine, frameInfo.frameSeconds);
            context.setOpacity(1.0f);
            drawMenuNoise(shell.style, context, engine.renderer);
            const bool isBack = frameInfo.input.wasKeyPressed(platform::Key::Escape) && !context.isCapturing()
                && shell.nextScreen == ShellScreen::None;
            context.end();

            if (isBack)
            {
                goBack(state, engine);
            }
        }

        void presentMenu(GameState& state, EngineContext& engine, const FrameInfo& frameInfo)
        {
            Shell& shell = state.shell;
            updateDevCapture(state.devCapture, engine.renderer, DevCaptureEvents{});

            if (engine.window.isCursorCaptured())
            {
                engine.window.setCursorCaptured(false);
            }

            shell.menuSeconds += frameInfo.frameSeconds;
            presentMenuBackdrop(state, engine, frameInfo.frameSeconds);
            engine.renderer.setScreenFade(1.0f - std::clamp(shell.menuSeconds / k_fadeFromBlackSeconds, 0.0f, 1.0f));
            drawMenus(state, engine, frameInfo, false);
        }
    } //namespace

    void initShell(GameState& state, EngineContext& engine)
    {
        Shell& shell = state.shell;
        shell.settingsFile = lockstep::render::graphicsConfigPath(engine.arguments);
        shell.settings = loadUserSettings(shell.settingsFile);
        shell.style = createMenuStyle(engine.renderer);
        applyUserSettings(shell.settings, state, engine);

#if LOCKSTEP_DEV_TOOLS
        shell.dev.openScreen = std::string(flagValue(engine.arguments, "--dev-menu").value_or(""));
        shell.dev.isAutoReady = std::any_of(engine.arguments.begin(), engine.arguments.end(), [](const char* argument)
        {
            return argument != nullptr && std::string_view(argument) == "--dev-auto-ready";
        });

        if (const std::optional<std::string_view> leave = flagValue(engine.arguments, "--dev-leave"))
        {
            shell.dev.leaveSeconds = static_cast<float>(std::atof(std::string(*leave).c_str()));
        }

        if (const std::optional<std::string_view> name = flagValue(engine.arguments, "--dev-name"))
        {
            shell.settings.playerName = std::string(name->substr(0, k_maxPlayerNameLength));
        }
#endif

        const bool isPresentGiven = std::any_of(engine.arguments.begin(), engine.arguments.end(), [](const char* argument)
        {
            return argument != nullptr && std::string_view(argument) == "--present";
        });

        if (!isPresentGiven && engine.renderer.presentMode() != shell.settings.presentMode)
        {
            if (auto changed = engine.renderer.setPresentMode(shell.settings.presentMode); !changed)
            {
                std::clog << "Game: present mode not changed: " << changed.error().message << '\n';
            }
        }

        shell.nameText = shell.settings.playerName;
        shell.portText = std::to_string(shell.settings.port);
        shell.addressText = shell.settings.address;
    }

    void enterMenu(GameState& state, EngineContext& engine)
    {
        Shell& shell = state.shell;
        shell.isInMenu = true;
        shell.isPauseOpen = false;
        shell.menuSeconds = 0.0f;
        shell.fade = 0.0f;
        showScreen(shell, ShellScreen::Main);
        engine.window.setCursorCaptured(false);
        loadMenuBackdrop(state, engine);

        const net::Role role = engine.session.role();

        if (role == net::Role::Host)
        {
            const std::optional<std::string_view> portText = flagValue(engine.arguments, "--port");
            const uint16_t port = portText ? static_cast<uint16_t>(std::atoi(std::string(*portText).c_str()))
                                           : shell.settings.port;

            if (auto opened = openHostLobby(shell.lobby, engine, shell.nameText, port); opened)
            {
                showScreen(shell, ShellScreen::Lobby);
            }
        }
        else if (role == net::Role::Client)
        {
            if (auto opened = openJoinLobby(shell.lobby, engine, shell.nameText,
                    flagValue(engine.arguments, "--connect").value_or("the host")); opened)
            {
                showScreen(shell, ShellScreen::Lobby);
            }
        }
    }

    bool tickShell(GameState& state, EngineContext& engine, const TickInfo& tick)
    {
        Shell& shell = state.shell;

        if (shell.isInMenu)
        {
            tickLobby(shell.lobby, engine, tick.tickSeconds);
#if LOCKSTEP_DEV_TOOLS
            tickDevLobby(shell, engine, tick.tickSeconds);
#endif

            if (shell.lobby.startLevel)
            {
                const std::string level = *shell.lobby.startLevel;
                beginGame(state, engine, level);
                return true;
            }

            return true;
        }

#if LOCKSTEP_DEV_TOOLS
        if (shell.dev.leaveSeconds > 0.0f)
        {
            shell.dev.leaveSeconds -= tick.tickSeconds;
            shell.isEndRequested = shell.isEndRequested || shell.dev.leaveSeconds <= 0.0f;
        }
#endif

        if (shell.isEndRequested)
        {
            shell.isEndRequested = false;
            returnToMenu(state, engine, "");
            return true;
        }

        if (shell.isClientGame && engine.session.state() == net::ConnectionState::Offline)
        {
            shell.isClientGame = false;
            std::clog << "Game: the connection to the host is gone, back to the menu\n";
            returnToMenu(state, engine, "CONNECTION TO THE HOST LOST");
            return true;
        }

        return false;
    }

    void presentShell(GameState& state, EngineContext& engine, const FrameInfo& frameInfo)
    {
        Shell& shell = state.shell;
#if LOCKSTEP_DEV_TOOLS
        applyDevScreen(state, engine);
#endif

        if (shell.isInMenu)
        {
            presentMenu(state, engine, frameInfo);
            enforceUserLook(shell.settings, engine);
            return;
        }

        const bool isEscape = frameInfo.input.wasKeyPressed(platform::Key::Escape);

        if (!shell.isPauseOpen && isEscape)
        {
            shell.isPauseOpen = true;
            shell.fade = 0.0f;
            showScreen(shell, ShellScreen::Pause);
            engine.window.setCursorCaptured(false);
            presentFrame(state, engine, frameInfo);
        }
        else
        {
            presentFrame(state, engine, frameInfo);

            if (shell.isPauseOpen)
            {
                drawMenus(state, engine, frameInfo, true);
            }
        }

        enforceUserLook(shell.settings, engine);
    }

    bool isShellBlockingInput(const Shell& shell)
    {
        return shell.isInMenu || shell.isPauseOpen;
    }

    void goToScreen(Shell& shell, ShellScreen screen)
    {
        if (screen == shell.screen && shell.nextScreen == ShellScreen::None)
        {
            return;
        }

        shell.nextScreen = screen;
    }
} //namespace game
