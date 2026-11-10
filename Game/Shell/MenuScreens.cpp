#include "Shell/ShellScreens.hpp"

#include "GameState.hpp"

#include <Lockstep/Platform/Window.hpp>

#include <algorithm>
#include <charconv>
#include <iostream>
#include <string>

namespace game
{
    namespace
    {
        namespace ui = lockstep::ui;

        using lockstep::app::EngineContext;
        using lockstep::app::GameState;

        constexpr float k_columnWidth{620.0f};
        constexpr float k_creditsSpeed{38.0f};
        constexpr std::string_view k_nameCharacters{
            "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789 _-."};
        constexpr std::string_view k_addressCharacters{"0123456789abcdefABCDEF.:[]"};
        constexpr std::string_view k_digits{"0123456789"};
        constexpr std::size_t k_maxAddressLength{45};
        constexpr std::size_t k_maxPortLength{5};

        std::optional<uint16_t> parsePort(std::string_view text)
        {
            uint32_t value = 0;
            const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);

            if (error != std::errc{} || end != text.data() + text.size() || value == 0 || value > 65535)
            {
                return std::nullopt;
            }

            return static_cast<uint16_t>(value);
        }

        void drawNotice(ui::UiContext& context, const std::string& notice, float y, float x)
        {
            if (!notice.empty())
            {
                context.text(notice, {x, y}, 22.0f, k_menuAccent);
            }
        }

        void saveSettings(Shell& shell)
        {
            if (auto saved = saveUserSettings(shell.settingsFile, shell.settings); !saved)
            {
                std::clog << "Game: settings not saved: " << saved.error().message << '\n';
            }
        }

        void rememberName(Shell& shell)
        {
            if (!shell.nameText.empty())
            {
                shell.settings.playerName = shell.nameText;
            }
        }

        float creditLineHeight(CreditKind kind)
        {
            switch (kind)
            {
                case CreditKind::Title:
                {
                    return 54.0f;
                }
                case CreditKind::Name:
                {
                    return 32.0f;
                }
                case CreditKind::Detail:
                {
                    return 26.0f;
                }
                case CreditKind::Gap:
                {
                    return 28.0f;
                }
            }

            return 26.0f;
        }
    } //namespace

    ScreenLayout screenLayout(const ui::UiContext& context, float width)
    {
        const math::Vec2 canvas = context.canvasSize();
        const float left = std::max(110.0f, canvas.x * 0.075f);

        return ScreenLayout{.title = {left, 150.0f},
            .column = ui::Rect{.min = {left, 300.0f}, .max = {left + width, canvas.y - 120.0f}}};
    }

    void drawMainScreen(GameState& state, EngineContext& engine)
    {
        Shell& shell = state.shell;
        ui::UiContext& context = shell.ui;
        const ScreenLayout layout = screenLayout(context, k_columnWidth);
        drawMenuTitle(context, layout.title, "L O C K S T E P", "CO-OP INCIDENT RESPONSE  //  FILE 0417");
        ui::Column column(layout.column, k_menuRowGap);

        if (context.button("main.host", column.next(k_menuRowHeight), "HOST GAME"))
        {
            shell.notice.clear();
            goToScreen(shell, ShellScreen::Host);
        }

        if (context.button("main.join", column.next(k_menuRowHeight), "JOIN GAME"))
        {
            shell.notice.clear();
            goToScreen(shell, ShellScreen::Join);
        }

        if (context.button("main.options", column.next(k_menuRowHeight), "OPTIONS"))
        {
            openOptions(shell, engine, ShellScreen::Main);
        }

        if (context.button("main.credits", column.next(k_menuRowHeight), "CREDITS"))
        {
            shell.creditsSeconds = 0.0f;
            goToScreen(shell, ShellScreen::Credits);
        }

        if (context.button("main.quit", column.next(k_menuRowHeight), "QUIT"))
        {
            engine.window.close();
        }

        drawNotice(context, shell.notice, column.cursor() + 20.0f, layout.column.min.x);
        const math::Vec2 canvas = context.canvasSize();
        context.text("UP / DOWN  ENTER  ESC", {canvas.x - 70.0f, canvas.y - 110.0f}, 18.0f, k_menuDim,
            lockstep::render::TextAlign::Right);
    }

    void drawHostScreen(GameState& state, EngineContext& engine)
    {
        Shell& shell = state.shell;
        ui::UiContext& context = shell.ui;
        const ScreenLayout layout = screenLayout(context, k_columnWidth + 140.0f);
        drawMenuTitle(context, layout.title, "HOST", "OPEN A CHANNEL FOR YOUR PARTNER (DIRECT IP)");
        ui::Column column(layout.column, k_menuRowGap);
        context.textField("host.name", column.next(k_menuRowHeight), "NAME", shell.nameText, k_maxPlayerNameLength,
            k_nameCharacters);
        context.textField("host.port", column.next(k_menuRowHeight), "PORT (UDP)", shell.portText, k_maxPortLength, k_digits);
        column.skip(18.0f);

        if (context.button("host.start", column.next(k_menuRowHeight), "HOST"))
        {
            const std::optional<uint16_t> port = parsePort(shell.portText);

            if (!port)
            {
                shell.notice = "PORT MUST BE 1-65535";
            }
            else if (auto opened = openHostLobby(shell.lobby, engine, shell.nameText.empty() ? "OPERATOR" : shell.nameText,
                         *port); !opened)
            {
                shell.notice = "CANNOT HOST: " + opened.error().message;
            }
            else
            {
                rememberName(shell);
                shell.settings.port = *port;
                saveSettings(shell);
                shell.notice.clear();
                goToScreen(shell, ShellScreen::Lobby);
            }
        }

        if (context.button("host.back", column.next(k_menuRowHeight), "BACK"))
        {
            goToScreen(shell, ShellScreen::Main);
        }

        drawNotice(context, shell.notice, column.cursor() + 20.0f, layout.column.min.x);
    }

    void drawJoinScreen(GameState& state, EngineContext& engine)
    {
        Shell& shell = state.shell;
        ui::UiContext& context = shell.ui;
        const ScreenLayout layout = screenLayout(context, k_columnWidth + 140.0f);
        drawMenuTitle(context, layout.title, "JOIN", "CONNECT TO YOUR PARTNER'S MACHINE (DIRECT IP)");
        ui::Column column(layout.column, k_menuRowGap);
        context.textField("join.name", column.next(k_menuRowHeight), "NAME", shell.nameText, k_maxPlayerNameLength,
            k_nameCharacters);
        context.textField("join.address", column.next(k_menuRowHeight), "HOST IP", shell.addressText, k_maxAddressLength,
            k_addressCharacters);
        context.textField("join.port", column.next(k_menuRowHeight), "PORT (UDP)", shell.portText, k_maxPortLength, k_digits);
        column.skip(18.0f);

        if (context.button("join.connect", column.next(k_menuRowHeight), "CONNECT"))
        {
            const std::optional<uint16_t> port = parsePort(shell.portText);

            if (!port || shell.addressText.empty())
            {
                shell.notice = shell.addressText.empty() ? "ENTER THE HOST'S IP ADDRESS" : "PORT MUST BE 1-65535";
            }
            else
            {
                const bool isIpv6 = shell.addressText.find(':') != std::string::npos;
                const std::string address = (isIpv6 ? "[" + shell.addressText + "]" : shell.addressText) + ":"
                    + std::to_string(*port);

                if (auto opened = openJoinLobby(shell.lobby, engine, shell.nameText.empty() ? "OPERATOR" : shell.nameText,
                        address); !opened)
                {
                    shell.notice = "CANNOT CONNECT: " + opened.error().message;
                }
                else
                {
                    rememberName(shell);
                    shell.settings.port = *port;
                    shell.settings.address = shell.addressText;
                    saveSettings(shell);
                    shell.notice.clear();
                    goToScreen(shell, ShellScreen::Lobby);
                }
            }
        }

        if (context.button("join.back", column.next(k_menuRowHeight), "BACK"))
        {
            goToScreen(shell, ShellScreen::Main);
        }

        drawNotice(context, shell.notice, column.cursor() + 20.0f, layout.column.min.x);
    }

    void drawCreditsScreen(GameState& state, EngineContext& engine, float frameSeconds)
    {
        Shell& shell = state.shell;
        ui::UiContext& context = shell.ui;

        if (shell.credits.empty())
        {
            shell.credits = loadCredits(LOCKSTEP_CREDITS_FILE, engine.files);
        }

        const math::Vec2 canvas = context.canvasSize();
        const ScreenLayout layout = screenLayout(context, k_columnWidth);
        drawMenuTitle(context, layout.title, "CREDITS", "");
        const ui::Rect area{.min = {layout.column.min.x, 260.0f},
            .max = {std::min(canvas.x - 110.0f, layout.column.min.x + 1500.0f), canvas.y - 170.0f}};
        float total = 0.0f;

        for (const CreditLine& line : shell.credits)
        {
            total += creditLineHeight(line.kind);
        }

        shell.creditsSeconds += frameSeconds;
        const float scroll = context.beginScroll("credits.list", area, total + area.height());

        const float automatic = std::fmod(shell.creditsSeconds * k_creditsSpeed, total + area.height());
        float y = area.max.y - automatic - scroll;

        for (const CreditLine& line : shell.credits)
        {
            const float height = creditLineHeight(line.kind);

            if (y + height > area.min.y && y < area.max.y)
            {
                switch (line.kind)
                {
                    case CreditKind::Title:
                    {
                        context.text(line.text, {area.min.x, y + 12.0f}, 32.0f, k_menuText);
                        context.fillRect(ui::rectAt({area.min.x, y + 50.0f}, {36.0f, 2.0f}), k_menuAccent);
                        break;
                    }
                    case CreditKind::Name:
                    {
                        context.text(line.text, {area.min.x, y + 6.0f}, 23.0f, k_menuText);
                        break;
                    }
                    case CreditKind::Detail:
                    {
                        context.text(line.text, {area.min.x + 24.0f, y + 2.0f}, 19.0f, k_menuDim);
                        break;
                    }
                    case CreditKind::Gap:
                    {
                        break;
                    }
                }
            }

            y += height;
        }

        context.endScroll();
        ui::Column column(ui::Rect{.min = {layout.column.min.x, canvas.y - 150.0f},
            .max = {layout.column.min.x + 300.0f, canvas.y - 90.0f}}, 0.0f);

        if (context.button("credits.back", column.next(k_menuRowHeight), "BACK"))
        {
            goToScreen(shell, ShellScreen::Main);
        }
    }

    void resumeGame(Shell& shell, EngineContext& engine)
    {
        shell.isPauseOpen = false;
        shell.screen = ShellScreen::None;
        shell.nextScreen = ShellScreen::None;
        engine.window.setCursorCaptured(true);
    }

    void drawPauseScreen(GameState& state, EngineContext& engine)
    {
        Shell& shell = state.shell;
        ui::UiContext& context = shell.ui;
        const ScreenLayout layout = screenLayout(context, k_columnWidth);
        drawMenuTitle(context, layout.title, "PAUSED", "THE OPERATION CONTINUES - YOUR PARTNER IS STILL OUT THERE");
        ui::Column column(layout.column, k_menuRowGap);

        if (context.button("pause.resume", column.next(k_menuRowHeight), "RESUME"))
        {
            resumeGame(shell, engine);
        }

        if (context.button("pause.options", column.next(k_menuRowHeight), "OPTIONS"))
        {
            openOptions(shell, engine, ShellScreen::Pause);
        }

        if (context.button("pause.menu", column.next(k_menuRowHeight), "QUIT TO MAIN MENU"))
        {
            shell.isEndRequested = true;
        }

        if (context.button("pause.quit", column.next(k_menuRowHeight), "QUIT GAME"))
        {
            engine.window.close();
        }
    }
} //namespace game
