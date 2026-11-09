#include "Station/StationDevPanel.hpp"

#if LOCKSTEP_DEV_TOOLS

#include "GameState.hpp"
#include "Simulation/StationSimulation.hpp"

#include <Lockstep/Net/Session.hpp>

#include <imgui.h>

#include <iostream>

namespace game
{
    namespace
    {
        using lockstep::app::EngineContext;
        using lockstep::app::GameState;

        void stationTable(const StationMission& mission)
        {
            const StationState& station = mission.state;
            ImGui::Text("Station     %s", toString(station.phase));

            if (station.phase == StationPhase::Briefing)
            {
                ImGui::SameLine();
                ImGui::Text("(%.0f s left on the host)", mission.briefingSeconds);
            }

            for (std::size_t player = 0; player < k_playerCount; ++player)
            {
                ImGui::Text("%s     armor %s  weapon %s  ammo %s  targets 0x%02x", playerName(player),
                    hasArmoryItem(station, player, ArmoryItem::Armor) ? "y" : "n",
                    hasArmoryItem(station, player, ArmoryItem::Weapon) ? "y" : "n",
                    hasArmoryItem(station, player, ArmoryItem::Ammo) ? "y" : "n", station.targetsHit[player]);
            }

            ImGui::Text("Targets     down 0x%02x of 0x%02x", station.targetsDown, rangeTargetMask(mission));
            ImGui::Text("Look        %s", mission.prompt.empty() ? "-" : mission.prompt.c_str());
        }
    } //namespace

    void stationDevSection(GameState& state, EngineContext& engine)
    {
        const LevelFlow& flow = state.levelFlow;

        if (flow.current.empty())
        {
            return;
        }

        ImGui::Separator();
        ImGui::Text("Level       %s", flow.current.c_str());
        const bool isHost = engine.session.role() != lockstep::net::Role::Client;

        if (isHost)
        {
            ImGui::Text("Next level  %s", flow.next.empty() ? "-" : flow.next.c_str());
        }

        if (isHost && state.room && state.room->carSpot && state.room->state.phase != MorningPhase::MorningComplete
            && ImGui::Button("Finish the morning"))
        {
            state.room->state.phase = MorningPhase::MorningComplete;
            std::clog << "Game: dev: morning complete\n";
        }

        if (!state.station)
        {
            return;
        }

        stationTable(*state.station);

        if (isHost && ImGui::Button("Advance the station"))
        {
            advanceStationForDev(state, engine);
        }
    }
} //namespace game

#endif
