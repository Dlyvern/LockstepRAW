#ifndef LOCKSTEP_CORE_FIXED_TIMESTEP_HPP
#define LOCKSTEP_CORE_FIXED_TIMESTEP_HPP

#include <cstdint>

namespace lockstep
{
    // Turns variable frame times into whole fixed-length simulation ticks plus an interpolation
    // alpha for rendering between the last two ticks. It never reads a clock: the caller passes
    // the elapsed time in.
    class FixedTimestep
    {
    public:
        // tickRate:         ticks per second. Must be greater than 0.
        // maxTicksPerFrame: most ticks a single advance() may return. Time beyond that is
        //                   dropped, so a long stall slows the simulation down instead of making
        //                   it fall further and further behind. Must be at least 1.
        FixedTimestep(double tickRate, uint32_t maxTicksPerFrame);

        // Adds elapsed time and takes as many whole ticks out of it as fit, up to the cap.
        // elapsedSeconds: time since the previous advance(). Negative values count as 0.
        // Returns:        the number of ticks to run now.
        [[nodiscard]] uint32_t advance(double elapsedSeconds);

        // Returns: how far the leftover time reaches into the next tick, in [0, 1). Rendering
        //          blends the previous and the current tick's state with it.
        [[nodiscard]] float alpha() const;

        // Returns: the length of one tick in seconds.
        [[nodiscard]] float tickSeconds() const;

    private:
        double m_tickSeconds;
        uint32_t m_maxTicksPerFrame;
        double m_accumulator{0.0};
    };
} //namespace lockstep

#endif //LOCKSTEP_CORE_FIXED_TIMESTEP_HPP
