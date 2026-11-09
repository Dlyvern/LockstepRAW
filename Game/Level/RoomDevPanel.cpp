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

        const char* toString(MorningPhase phase)
        {
            return phase == MorningPhase::Apartment ? "apartment"
                : phase == MorningPhase::GoToCar ? "go to the car" : "complete";
        }

        const char* heldBy(const ApartmentRoom& room, uint8_t toy)
        {
            return toy < room.toys.size() ? room.toys[toy].label.c_str() : "-";
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
                ImGui::Text("%s     locker %3.0f%%  badge %s  sidearm %s  mirror %s", playerName(player),
                    room.lockerOpen[player] * 100.0f, room.hasBadge[player] ? "y" : "n",
                    room.hasSidearm[player] ? "y" : "n", room.hasUsedMirror[player] ? "y" : "n");
            }

            ImGui::Text("Front door  %s %3.0f%%", toString(room.door), room.doorOpen * 100.0f);
            ImGui::Text("Street door %s %3.0f%%", room.isStreetDoorOpen ? "open" : "closed", room.streetDoorOpen * 100.0f);
            ImGui::Text("Morning     %s", toString(room.phase));
        }

        void echoControls(GameState& state)
        {
            const std::filesystem::path path = state.room ? state.room->echoPath : defaultEchoPath();

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
            const ApartmentRoom& room = *state.room;
            ImGui::Text("Look        %s", room.prompt.empty() ? "-" : room.prompt.c_str());
            stateTable(room.state);
            ImGui::Text("Carrying    Player1 %s, Player2 %s", heldBy(room, room.state.heldToy[0]),
                heldBy(room, room.state.heldToy[1]));
        }

        echoControls(state);
    }
} //namespace game

#endif
