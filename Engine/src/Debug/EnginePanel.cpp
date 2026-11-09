#include "EnginePanel.hpp"

#include "Lockstep/Physics/PhysicsWorld.hpp"
#include "Lockstep/Render/PresentMode.hpp"

#include <imgui.h>

#include <cstdint>

namespace lockstep::debug
{
    namespace
    {
        constexpr double k_bytesPerMegabyte{1024.0 * 1024.0};
        constexpr float k_millisecondsPerSecond{1000.0f};
        constexpr ImVec2 k_panelPosition{10.0f, 10.0f};

        double toMegabytes(uint64_t bytes)
        {
            return static_cast<double>(bytes) / k_bytesPerMegabyte;
        }

        void drawTimeRow(const char* label, const FrameTimeSummary& summary)
        {
            ImGui::Text("%-22s %6.2f %6.2f %6.2f", label, summary.minimum, summary.average, summary.maximum);
        }

        void setTooltip(const char* text)
        {
            if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip("%s", text);
            }
        }
    } //namespace

    void drawEnginePanel(const render::RendererState& renderer, const FrameTimeHistories& frameTimes)
    {
        const FrameTimeSummary frame = frameTimes.frame.summary();
        const float framesPerSecond = frame.average > 0.0f ? k_millisecondsPerSecond / frame.average : 0.0f;
        const vela::graphics::FrameStats frameStats = renderer.graph->getFrameStats();
        const vela::core::SwapchainInfo swapchain = renderer.context.getSwapchainInfo();
        const vela::core::MemoryStats memoryStats = renderer.context.getMemoryStats();

        ImGui::SetNextWindowPos(k_panelPosition, ImGuiCond_FirstUseEver);

        if (ImGui::Begin("Engine", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::TextUnformatted(renderer.context.getDeviceInfo().name.c_str());
            ImGui::Text("Present    %s (asked: %s)", vela::core::toString(swapchain.actualVSync),
                render::toString(renderer.requestedPresentMode));
            ImGui::Separator();
            ImGui::Text("FPS        %.0f (avg over %u frames)", framesPerSecond, frame.count);
            ImGui::TextDisabled("%-22s %6s %6s %6s", "ms, last frames", "min", "avg", "max");
            drawTimeRow("Frame", frame);
            drawTimeRow("CPU work", frameTimes.cpuWork.summary());
            setTooltip("Run loop time outside Vela's RenderGraph::execute(): input, ticks, game, "
                "dev tools, render setup.\nExcludes the vsync wait, and also Vela's command recording, "
                "submit and present.");
            drawTimeRow("Vela frame incl. vsync", frameTimes.velaFrame.summary());
            setTooltip("Vela FrameStats::cpuFrameMs: RenderGraph::execute(), including the wait for "
                "a free frame and swapchain image (vsync).");

            if (frameStats.gpuTimingValid)
            {
                ImGui::Text("GPU frame  %.2f ms (last)", frameStats.gpuFrameMs);
            }
            else
            {
                ImGui::TextUnformatted("GPU frame  n/a");
            }

            ImGui::Separator();
            ImGui::Text("VRAM       %.1f / %.1f MB%s", toMegabytes(memoryStats.deviceBytesUsed),
                toMegabytes(memoryStats.deviceBytesBudget), memoryStats.budgetFromDriver ? "" : " (estimated)");
            ImGui::Text("Allocations %u", memoryStats.allocationCount);
            ImGui::Separator();
            ImGui::Text("Validation messages %u", renderer.context.getValidationMessageCount());
            ImGui::TextDisabled("F1 toggles the dev tools");
        }

        ImGui::End();
    }

    void drawPhysicsSection(const physics::PhysicsWorld& physics)
    {
        const physics::PhysicsStats stats = physics.stats();

        if (ImGui::Begin("Engine", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::Separator();
            ImGui::TextUnformatted("Physics");
            ImGui::Text("Bodies      %u", stats.bodyCount);
            ImGui::Text("Controllers %u", stats.controllerCount);
            ImGui::Text("Step        %.2f ms (%u x %.2f ms sim)", stats.lastStepMilliseconds, stats.lastSubsteps,
                stats.lastSubsteps > 0
                    ? static_cast<double>(stats.lastStepSeconds * k_millisecondsPerSecond) / stats.lastSubsteps
                    : 0.0);
            ImGui::Text("PhysX errors %u, warnings %u", stats.errorCount, stats.warningCount);
        }

        ImGui::End();
    }
} //namespace lockstep::debug
