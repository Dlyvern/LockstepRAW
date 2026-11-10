#include "Shell/ShellScreens.hpp"

#include "GameState.hpp"

#include <Lockstep/Net/Session.hpp>

#include <string>

namespace game
{
    namespace
    {
        namespace ui = lockstep::ui;

        using lockstep::app::EngineContext;
        using lockstep::app::GameState;

        constexpr float k_columnWidth{760.0f};
        constexpr float k_seatHeight{64.0f};

        void drawSeat(ui::UiContext& context, const ui::Rect& rect, std::string_view role, const LobbyPlayer& player,
            bool isLocal)
        {
            drawMenuPanel(context, rect);
            const float textY = rect.center().y - 13.0f;
            context.text(role, {rect.min.x + 18.0f, textY}, 20.0f, k_menuDim);

            if (!player.isPresent)
            {
                context.text("-- EMPTY --", {rect.min.x + 150.0f, textY}, 25.0f, k_menuDim);
                return;
            }

            context.text(player.name + (isLocal ? "  (YOU)" : ""), {rect.min.x + 150.0f, textY}, 25.0f, k_menuText);
            context.text(player.isReady ? "READY" : "NOT READY", {rect.max.x - 18.0f, textY}, 25.0f,
                player.isReady ? k_menuText : k_menuAccent, lockstep::render::TextAlign::Right);

            if (player.isReady)
            {
                context.fillRect(ui::rectAt({rect.max.x - 150.0f, rect.max.y - 10.0f}, {132.0f, 2.0f}), k_menuText);
            }
        }
    } //namespace

    void drawLobbyScreen(GameState& state, EngineContext& engine)
    {
        Shell& shell = state.shell;
        Lobby& lobby = shell.lobby;
        ui::UiContext& context = shell.ui;
        const ScreenLayout layout = screenLayout(context, k_columnWidth);
        drawMenuTitle(context, layout.title, "LOBBY", lobby.isHost ? "YOU HOST - PICK THE OPERATION, START WHEN READY"
                                                                   : "THE HOST PICKS THE OPERATION");
        ui::Column column(layout.column, k_menuRowGap);
        drawSeat(context, column.next(k_seatHeight), "HOST", lobby.players[0], lobby.isHost);
        drawSeat(context, column.next(k_seatHeight), "PARTNER", lobby.players[1], !lobby.isHost);
        context.text(lobby.status, {layout.column.min.x, column.cursor() + 4.0f}, 20.0f,
            lobby.isFailed ? k_menuAccent : k_menuDim);
        column.skip(48.0f);

        if (lobby.isHost)
        {
            std::size_t index = lobby.levelIndex;

            if (context.cycler("lobby.level", column.next(k_menuRowHeight), "OPERATION", index, lobby.levelLabels))
            {
                setLobbyLevel(lobby, engine, index);
            }
        }
        else
        {
            const ui::Rect row = column.next(k_menuRowHeight);
            const std::string level = lobby.levelLabels.empty() ? std::string("-") : lobby.levelLabels[lobby.levelIndex];
            context.text("OPERATION", {row.min.x + 16.0f, row.center().y - 12.5f}, 25.0f, k_menuDim);
            context.text(level, {row.max.x - 16.0f, row.center().y - 12.5f}, 25.0f, k_menuText,
                lockstep::render::TextAlign::Right);
        }

        if (!lobby.isFailed)
        {
            const bool isReady = lobby.players[lobby.isHost ? 0 : 1].isReady;

            if (context.button("lobby.ready", column.next(k_menuRowHeight), isReady ? "NOT READY" : "READY"))
            {
                setLobbyReady(lobby, engine, !isReady);
            }
        }

        if (lobby.isHost)
        {
            const bool canStart = canStartLobby(lobby, engine);
            const bool isSolo = !lobby.players[1].isPresent;
            const std::string label = !canStart ? "START (WAITING FOR READY)" : (isSolo ? "START SOLO" : "START");

            if (context.button("lobby.start", column.next(k_menuRowHeight), label) && canStart)
            {
                startLobby(lobby, engine);
            }
        }

        if (context.button("lobby.leave", column.next(k_menuRowHeight), "LEAVE"))
        {
            closeLobby(lobby, engine);
            goToScreen(shell, ShellScreen::Main);
        }
    }
} //namespace game
