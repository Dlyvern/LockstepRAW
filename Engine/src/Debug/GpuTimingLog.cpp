#include "GpuTimingLog.hpp"

#include "Lockstep/Core/Log.hpp"

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

    void recordGpuTimings(GpuTimingLog& log, const vela::graphics::RenderGraph& graph)
    {
        const vela::graphics::FrameStats stats = graph.getFrameStats();

        if (!log.enabled || !stats.gpuTimingValid)
        {
            return;
        }

        log.frameMs += stats.gpuFrameMs;

        for (const vela::graphics::PassTiming& timing : graph.getPassTimings())
        {
            log.passMs[timing.name] += timing.gpuMs;
        }

        if (++log.frames < k_gpuTimingLogFrames)
        {
            return;
        }

        const float frames = static_cast<float>(log.frames);
        std::string line = "GPU ms over " + std::to_string(log.frames) + " frames: frame " + formatMs(log.frameMs / frames);

        for (const auto& [name, sum] : log.passMs)
        {
            line += ", " + name + " " + formatMs(sum / frames);
        }

        lockstep::log(LogLevel::Info, "Render", line);
        log.frames = 0;
        log.frameMs = 0.0f;
        log.passMs.clear();
    }
} //namespace lockstep::debug
