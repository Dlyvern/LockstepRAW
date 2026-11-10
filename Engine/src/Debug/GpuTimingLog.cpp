#include "GpuTimingLog.hpp"

#include "Lockstep/Core/Log.hpp"

#include "Render/RendererState.hpp"

#include <Vela/Graphics/RenderGraph.hpp>

#include <cstdio>

namespace lockstep::debug
{
    namespace
    {
        std::string formatMs(float milliseconds)
        {
            char text[16]{};
            std::snprintf(text, sizeof(text), "%.3f", static_cast<double>(milliseconds));

            return text;
        }
    } //namespace

    void recordGpuTimings(GpuTimingLog& log, const vela::graphics::RenderGraph& graph, const render::FrameStats& frame)
    {
        const vela::graphics::FrameStats stats = graph.getFrameStats();

        if (!log.enabled || !stats.gpuTimingValid)
        {
            return;
        }

        log.frameMs += stats.gpuFrameMs;
        log.cpuWorkMs += stats.cpuWorkMs;
        log.cpuWaitMs += stats.cpuWaitMs;
        log.visibleDraws += frame.visible;
        log.shadowDraws += frame.shadowDraws;

        for (const vela::graphics::PassTiming& timing : graph.getPassTimings())
        {
            log.passMs[timing.name] += timing.gpuMs;
        }

        if (++log.frames < k_gpuTimingLogFrames)
        {
            return;
        }

        const float frames = static_cast<float>(log.frames);
        std::string line = "GPU ms over " + std::to_string(log.frames) + " frames: frame " + formatMs(log.frameMs / frames)
            + " (CPU record " + formatMs(log.cpuWorkMs / frames) + ", wait " + formatMs(log.cpuWaitMs / frames)
            + ", draws " + std::to_string(log.visibleDraws / log.frames) + " visible " + std::to_string(log.shadowDraws / log.frames)
            + " shadow)";

        for (const auto& [name, sum] : log.passMs)
        {
            line += ", " + name + " " + formatMs(sum / frames);
        }

        lockstep::log(LogLevel::Info, "Render", line);
        log.frames = 0;
        log.frameMs = 0.0f;
        log.cpuWorkMs = 0.0f;
        log.cpuWaitMs = 0.0f;
        log.visibleDraws = 0;
        log.shadowDraws = 0;
        log.passMs.clear();
    }
} //namespace lockstep::debug
