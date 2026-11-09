#include "EnginePanel.hpp"

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
    } //namespace

    void drawEnginePanel(const render::RendererState& renderer)
    {
        const float framesPerSecond = ImGui::GetIO().Framerate;
        const float frameMilliseconds = framesPerSecond > 0.0f ? k_millisecondsPerSecond / framesPerSecond : 0.0f;
        const vela::graphics::FrameStats frameStats = renderer.graph->getFrameStats();
        const vela::core::MemoryStats memoryStats = renderer.context.getMemoryStats();

        ImGui::SetNextWindowPos(k_panelPosition, ImGuiCond_FirstUseEver);

        if (ImGui::Begin("Engine", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::TextUnformatted(renderer.context.getDeviceInfo().name.c_str());
            ImGui::Separator();
            ImGui::Text("FPS        %.0f (%.2f ms)", framesPerSecond, frameMilliseconds);
            ImGui::Text("CPU frame  %.2f ms", frameStats.cpuFrameMs);

            if (frameStats.gpuTimingValid)
            {
                ImGui::Text("GPU frame  %.2f ms", frameStats.gpuFrameMs);
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
} //namespace lockstep::debug
