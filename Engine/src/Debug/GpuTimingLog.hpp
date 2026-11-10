#ifndef LOCKSTEP_DEBUG_GPU_TIMING_LOG_HPP
#define LOCKSTEP_DEBUG_GPU_TIMING_LOG_HPP

#include <cstdint>
#include <map>
#include <string>

namespace vela::graphics
{
    class RenderGraph;
} //namespace vela::graphics

namespace lockstep::render
{
    struct FrameStats;
} //namespace lockstep::render

namespace lockstep::debug
{
    // Averages of the render graph's GPU pass timings, logged every k_gpuTimingLogFrames frames
    // when the run loop was started with `--dev-gpu-timings` (for measuring a feature's cost
    // without a person at the dev panel).
    // enabled: logging is on.
    // frames:  frames accumulated since the last log line.
    // frameMs: sum of the GPU frame times.
    // cpuWorkMs: sum of the CPU time RenderGraph::execute() spent recording, submitting, presenting.
    // cpuWaitMs: sum of the CPU time it spent blocked on the GPU or the swapchain.
    // visibleDraws, shadowDraws: sums of the renderer's visible and shadow-map draws.
    // passMs:  per pass, the sum of its GPU times.
    struct GpuTimingLog
    {
        bool enabled{false};
        uint32_t frames{0};
        float frameMs{0.0f};
        float cpuWorkMs{0.0f};
        float cpuWaitMs{0.0f};
        uint64_t visibleDraws{0};
        uint64_t shadowDraws{0};
        std::map<std::string, float> passMs;
    };

    // Frames averaged into one log line.
    inline constexpr uint32_t k_gpuTimingLogFrames{240};

    // Adds the newest finished frame's timings and logs the averages ("Render" channel) once
    // enough frames were added. Does nothing while the log is off or timings are not valid.
    // log:   the accumulated timings, updated.
    // graph: the render graph.
    // frame: the renderer's counts of the frame just rendered.
    void recordGpuTimings(GpuTimingLog& log, const vela::graphics::RenderGraph& graph, const render::FrameStats& frame);
} //namespace lockstep::debug

#endif //LOCKSTEP_DEBUG_GPU_TIMING_LOG_HPP
