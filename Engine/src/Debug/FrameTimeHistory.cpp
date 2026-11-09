#include "FrameTimeHistory.hpp"

#include <algorithm>

namespace lockstep::debug
{
    void FrameTimeHistory::add(float milliseconds)
    {
        m_samples[m_next] = milliseconds;
        m_next = (m_next + 1) % k_capacity;
        m_count = std::min(m_count + 1, k_capacity);
    }

    FrameTimeSummary FrameTimeHistory::summary() const
    {
        if (m_count == 0)
        {
            return {};
        }

        float minimum = m_samples[0];
        float maximum = m_samples[0];
        double total = 0.0;

        for (size_t index = 0; index < m_count; ++index)
        {
            const float sample = m_samples[index];
            minimum = std::min(minimum, sample);
            maximum = std::max(maximum, sample);
            total += sample;
        }

        return FrameTimeSummary{
            .minimum = minimum,
            .average = static_cast<float>(total / static_cast<double>(m_count)),
            .maximum = maximum,
            .count = static_cast<uint32_t>(m_count)};
    }
} //namespace lockstep::debug
