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
    // frame:      wall time of one run-loop iteration (work + render).
    // engineWork: CPU time the Engine and Game spend on the frame: the iteration minus the time
    //             inside Vela's RenderGraph::execute().
    // velaWork:   Vela's FrameStats::cpuWorkMs: recording, submitting and presenting.
    // velaWait:   Vela's FrameStats::cpuWaitMs: blocked on a frame in flight or a swapchain image
    //             (where vsync waits).
    // gpu:        Vela's FrameStats::gpuFrameMs of the newest finished frame, when valid.
    struct FrameTimeHistories
    {
        FrameTimeHistory frame;
        FrameTimeHistory engineWork;
        FrameTimeHistory velaWork;
        FrameTimeHistory velaWait;
        FrameTimeHistory gpu;
    };

} //namespace lockstep::debug

#endif //LOCKSTEP_DEBUG_FRAME_TIME_HISTORY_HPP
