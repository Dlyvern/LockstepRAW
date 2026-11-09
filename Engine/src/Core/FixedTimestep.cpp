#include "Lockstep/Core/FixedTimestep.hpp"

#include <algorithm>
#include <cassert>

namespace lockstep
{
    FixedTimestep::FixedTimestep(double tickRate, uint32_t maxTicksPerFrame)
        : m_tickSeconds(1.0 / tickRate),
          m_maxTicksPerFrame(std::max(maxTicksPerFrame, 1u))
    {
        assert(tickRate > 0.0 && "FixedTimestep: tickRate must be greater than 0");
        assert(maxTicksPerFrame >= 1 && "FixedTimestep: maxTicksPerFrame must be at least 1");
    }

    uint32_t FixedTimestep::advance(double elapsedSeconds)
    {
        m_accumulator += std::max(elapsedSeconds, 0.0);

        uint32_t ticks = 0;

        while (m_accumulator >= m_tickSeconds && ticks < m_maxTicksPerFrame)
        {
            m_accumulator -= m_tickSeconds;
            ++ticks;
        }

        if (m_accumulator >= m_tickSeconds)
        {
            m_accumulator = 0.0;
        }

        return ticks;
    }

    float FixedTimestep::alpha() const
    {
        return static_cast<float>(std::clamp(m_accumulator / m_tickSeconds, 0.0, 1.0));
    }

    float FixedTimestep::tickSeconds() const
    {
        return static_cast<float>(m_tickSeconds);
    }
} //namespace lockstep
