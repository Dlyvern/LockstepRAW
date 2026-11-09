#include "NetPanel.hpp"

#include <imgui.h>

#include <algorithm>
#include <cstdint>
#include <limits>
#include <optional>

namespace lockstep::debug
{
    namespace
    {
        constexpr ImVec2 k_panelPosition{10.0f, 220.0f};
        constexpr int k_maxLagMilliseconds{500};
        constexpr float k_maxLossPercent{50.0f};
        constexpr float k_percent{100.0f};
        constexpr float k_bytesPerKilobyte{1024.0f};
        constexpr float k_portFieldWidth{90.0f};
        constexpr ImVec4 k_errorColor{1.0f, 0.4f, 0.4f, 1.0f};
        constexpr float k_millisecondsPerTick{1000.0f / 60.0f};

        void remember(NetPanelState& panel, const Status& status)
        {
            panel.lastError = status ? std::string() : status.error().message;
        }

        void drawOfflineControls(NetPanelState& panel, net::Session& session)
        {
            ImGui::SetNextItemWidth(k_portFieldWidth);
            ImGui::InputInt("Port", &panel.port, 0);
            panel.port = std::clamp(panel.port, 1, static_cast<int>(std::numeric_limits<uint16_t>::max()));

            if (ImGui::Button("Host"))
            {
                remember(panel, session.host(static_cast<uint16_t>(panel.port)));
            }

            ImGui::InputText("Address", panel.address.data(), panel.address.size());

            if (ImGui::Button("Connect"))
            {
                remember(panel, session.connect(panel.address.data()));
            }
        }

        void drawConnectionStats(const net::Session& session)
        {
            const std::optional<net::ConnectionStats> stats = session.stats();

            if (!stats)
            {
                return;
            }

            ImGui::Text("Ping        %.0f ms", stats->pingMilliseconds);
            ImGui::Text("Loss        %.1f %%", stats->packetLoss * k_percent);
            ImGui::Text("Out / in    %.1f / %.1f KB/s", stats->sendBytesPerSecond / k_bytesPerKilobyte,
                stats->receiveBytesPerSecond / k_bytesPerKilobyte);
            ImGui::Text("Unacked     %u B", stats->pendingReliableBytes);

            const std::optional<uint64_t> remoteTick = session.remoteTick();

            if (remoteTick)
            {
                ImGui::Text("Remote tick %llu", static_cast<unsigned long long>(*remoteTick));
            }

            ImGui::Text("Heartbeats  %llu", static_cast<unsigned long long>(session.heartbeatsReceived()));
        }

        void drawSimulatedConditions(NetPanelState& panel, net::Network& network)
        {
            net::SimulatedConditions conditions = network.simulatedConditions();
            int lagMilliseconds = static_cast<int>(conditions.lagMilliseconds);

            ImGui::TextUnformatted("Simulated (outgoing, this process)");
            const bool lagChanged = ImGui::SliderInt("Lag ms", &lagMilliseconds, 0, k_maxLagMilliseconds);
            const bool lossChanged = ImGui::SliderFloat("Loss %", &conditions.lossPercent, 0.0f, k_maxLossPercent, "%.1f");

            if (lagChanged || lossChanged)
            {
                conditions.lagMilliseconds = static_cast<uint32_t>(std::max(lagMilliseconds, 0));
                remember(panel, network.setSimulatedConditions(conditions));
            }
        }

        void drawReplication(net::Session& session, net::Replication& replication)
        {
            const net::ReplicationStats stats = replication.stats();
            const net::ReplicatedTimeScale worldScale = replication.worldTimeScale();

            ImGui::TextUnformatted("Replication");
            ImGui::Text("World scale x%.2f since tick %llu", worldScale.scale, static_cast<unsigned long long>(worldScale.tick));

            if (session.role() == net::Role::Host)
            {
                ImGui::Text("Snapshots   %llu sent", static_cast<unsigned long long>(stats.snapshotsSent));
                ImGui::Text("Inputs      %llu received", static_cast<unsigned long long>(stats.inputsReceived));
                return;
            }

            if (session.role() != net::Role::Client)
            {
                return;
            }

            ImGui::Text("Snapshots   %llu received, %zu buffered", static_cast<unsigned long long>(stats.snapshotsReceived),
                stats.bufferedSnapshots);
            ImGui::Text("Inputs      %llu sent", static_cast<unsigned long long>(stats.inputsSent));
            ImGui::Text("Mirrored    %zu entities", stats.mirroredEntities);

            if (stats.newestSnapshotTick && stats.renderTick)
            {
                ImGui::Text("Showing     tick %.1f (%.1f behind newest)", *stats.renderTick,
                    static_cast<double>(*stats.newestSnapshotTick) - *stats.renderTick);
            }

            float delay = replication.interpolationDelay();

            if (ImGui::SliderFloat("Delay ticks", &delay, 0.0f, net::k_maxInterpolationDelayTicks, "%.1f"))
            {
                replication.setInterpolationDelay(delay);
            }

            ImGui::Text("Delay       %.0f ms at 60 Hz", delay * k_millisecondsPerTick);
        }
    } //namespace

    void drawNetPanel(NetPanelState& panel, net::Network& network, net::Session& session, net::Replication& replication)
    {
        ImGui::SetNextWindowPos(k_panelPosition, ImGuiCond_FirstUseEver);

        if (ImGui::Begin("Net", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::Text("Role        %s", net::toString(session.role()));
            ImGui::Text("State       %s", net::toString(session.state()));

            if (!session.lastEvent().empty())
            {
                ImGui::TextUnformatted(session.lastEvent().c_str());
            }

            ImGui::Separator();

            if (session.state() == net::ConnectionState::Offline)
            {
                drawOfflineControls(panel, session);
            }
            else if (ImGui::Button("Disconnect"))
            {
                session.disconnect();
                panel.lastError.clear();
            }

            if (!panel.lastError.empty())
            {
                ImGui::TextColored(k_errorColor, "%s", panel.lastError.c_str());
            }

            drawConnectionStats(session);
            ImGui::Separator();
            drawSimulatedConditions(panel, network);
            ImGui::Separator();
            drawReplication(session, replication);
        }

        ImGui::End();
    }
} //namespace lockstep::debug
