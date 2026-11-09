#include "EnginePanel.hpp"

#include "Lockstep/Core/Log.hpp"
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
        constexpr render::PresentMode k_presentModes[]{render::PresentMode::VSync, render::PresentMode::LowLatency,
            render::PresentMode::Uncapped};

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

        void drawPresentModeSelector(render::RendererState& renderer, const vela::core::SwapchainInfo& swapchain)
        {
            if (ImGui::BeginCombo("Present", render::toString(renderer.presentMode)))
            {
                for (const render::PresentMode mode : k_presentModes)
                {
                    if (ImGui::Selectable(render::toString(mode), mode == renderer.presentMode)
                        && mode != renderer.presentMode)
                    {
                        if (auto changed = renderer.setPresentMode(mode); !changed)
                        {
                            log(LogLevel::Error, "DevTools", "present mode change failed: " + changed.error().message);
                        }
                    }
                }

                ImGui::EndCombo();
            }

            ImGui::TextDisabled("in use: %s", vela::core::toString(swapchain.actualVSync));
        }
    } //namespace

    void drawEnginePanel(render::RendererState& renderer, const FrameTimeHistories& frameTimes)
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
            drawPresentModeSelector(renderer, swapchain);
            ImGui::Separator();
            ImGui::Text("FPS        %.0f (avg over %u frames)", framesPerSecond, frame.count);
            ImGui::TextDisabled("%-22s %6s %6s %6s", "ms, last frames", "min", "avg", "max");
            drawTimeRow("Frame", frame);
            drawTimeRow("CPU Engine + Game", frameTimes.engineWork.summary());
            setTooltip("Run loop time outside Vela's RenderGraph::execute(): input, ticks, game, "
                "dev tools, render setup.");
            drawTimeRow("CPU Vela work", frameTimes.velaWork.summary());
            setTooltip("Vela FrameStats::cpuWorkMs: recording every pass, submitting and presenting.");
            drawTimeRow("CPU Vela wait", frameTimes.velaWait.summary());
            setTooltip("Vela FrameStats::cpuWaitMs: blocked on a frame in flight or a swapchain image. "
                "With vsync this is where the frame waits for the display.");

            if (frameStats.gpuTimingValid)
            {
                drawTimeRow("GPU", frameTimes.gpu.summary());
            }
            else
            {
                ImGui::TextUnformatted("GPU        n/a");
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
