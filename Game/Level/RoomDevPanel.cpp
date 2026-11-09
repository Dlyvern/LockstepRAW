#include "Level/RoomDevPanel.hpp"

#if LOCKSTEP_DEV_TOOLS

#include "GameState.hpp"
#include "Simulation/RoomSimulation.hpp"

#include <imgui.h>

#include <iostream>

namespace game
{
    namespace
    {
        using lockstep::app::GameState;

        const char* toString(CoffeePhase phase)
        {
            return phase == CoffeePhase::Idle ? "idle" : phase == CoffeePhase::Brewing ? "brewing" : "ready";
        }

        const char* toString(DoorPhase phase)
        {
            return phase == DoorPhase::Closed ? "closed" : phase == DoorPhase::Opening ? "opening" : "open";
        }

        const char* takenBy(uint8_t player)
        {
            return player == k_nobody ? "on the counter" : playerName(player);
        }

        void stateTable(const RoomState& room)
        {
            ImGui::Text("Alarm       %s", room.isAlarmRinging ? "ringing"
                : room.alarmSilencedBy == k_nobody ? "off" : playerName(room.alarmSilencedBy));
            ImGui::Text("Coffee      %s, mug A %s, mug B %s", toString(room.coffee), takenBy(room.mugTakenBy[0]),
                takenBy(room.mugTakenBy[1]));
            ImGui::Text("Radio       %s", room.isRadioOn ? "on" : "off");

            for (std::size_t player = 0; player < k_playerCount; ++player)
            {
                ImGui::Text("%s     locker %3.0f%%  badge %s  sidearm %s  mirror %s  at door %s", playerName(player),
                    room.lockerOpen[player] * 100.0f, room.hasBadge[player] ? "y" : "n",
                    room.hasSidearm[player] ? "y" : "n", room.hasUsedMirror[player] ? "y" : "n",
                    room.isWaitingAtDoor[player] ? "y" : "n");
            }

            ImGui::Text("Front door  %s %3.0f%%", toString(room.door), room.doorOpen * 100.0f);
        }

        void echoControls(GameState& state)
        {
            const std::filesystem::path path = state.room ? state.room->echoPath : std::filesystem::path(k_defaultEchoPath);

            if (state.room)
            {
                ImGui::Text("Echo        %s, %zu ticks, %zu interactions", state.room->isRecording ? "recording" : "saved",
                    state.room->recording.frames.size(), state.room->recording.events.size());
            }

            if (ImGui::Button("Play echo"))
            {
                if (auto played = playEcho(state, path); !played)
                {
                    std::clog << "Game: echo not played: " << played.error().message << '\n';
                }
            }

            ImGui::SameLine();

            if (ImGui::Button("Stop echo"))
            {
                state.echo.reset();
            }

            ImGui::SameLine();
            ImGui::TextDisabled("%s", path.string().c_str());
        }
    } //namespace

    void roomDevSection(GameState& state)
    {
        if (!state.room && !state.echo)
        {
            return;
        }

        ImGui::Separator();

        if (state.room)
        {
            ImGui::Text("Look        %s", state.room->prompt[0] != '\0' ? state.room->prompt : "-");
            stateTable(state.room->state);
        }

        echoControls(state);
    }
} //namespace game

#endif
