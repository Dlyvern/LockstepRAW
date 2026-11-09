#ifndef LOCKSTEP_DEBUG_FRAME_TIME_HISTORY_HPP
#define LOCKSTEP_DEBUG_FRAME_TIME_HISTORY_HPP

#include <array>
#include <cstddef>
#include <cstdint>

namespace lockstep::debug
{
    // Minimum, mean and maximum of the samples in a FrameTimeHistory, in milliseconds.
    // count: number of samples summarized; 0 means no data and every figure is 0.
    struct FrameTimeSummary
    {
        float minimum{0.0f};
        float average{0.0f};
        float maximum{0.0f};
        uint32_t count{0};
    };

    // The last k_capacity samples of one per-frame duration, for min/avg/max readouts.
    class FrameTimeHistory
    {
    public:
        static constexpr size_t k_capacity{240};

        // Adds one sample, replacing the oldest once the history is full.
        // milliseconds: the duration to record.
        void add(float milliseconds);

        // Returns: min/avg/max over the samples currently held.
        [[nodiscard]] FrameTimeSummary summary() const;

    private:
        std::array<float, k_capacity> m_samples{};
        size_t m_next{0};
        size_t m_count{0};
    };

    // Per-frame durations the Engine panel shows, each over the last FrameTimeHistory::k_capacity
    // frames.
    // frame:     wall time of one run-loop iteration (work + render).
    // cpuWork:   CPU time spent doing frame work: the iteration minus the time inside Vela's
    //            RenderGraph::execute(). That call blocks on the frame fence and on swapchain
    //            image acquire (the vsync wait) and also records, submits and presents; Engine
    //            can't split those apart, so command recording inside the graph is left out too.
    // velaFrame: Vela's FrameStats::cpuFrameMs, the duration of RenderGraph::execute(),
    //            including the vsync wait.
    struct FrameTimeHistories
    {
        FrameTimeHistory frame;
        FrameTimeHistory cpuWork;
        FrameTimeHistory velaFrame;
    };
} //namespace lockstep::debug

#endif //LOCKSTEP_DEBUG_FRAME_TIME_HISTORY_HPP
