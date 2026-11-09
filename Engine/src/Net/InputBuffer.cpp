#include "InputBuffer.hpp"

#include <algorithm>

namespace lockstep::net
{
    namespace
    {
        constexpr std::size_t k_targetDepth{2};
        constexpr std::size_t k_catchUpDepth{5};
        constexpr std::size_t k_maxDepth{30};
        constexpr std::size_t k_catchUpInputs{2};
    } //namespace

    void InputBuffer::insert(const InputMessage& input)
    {
        if (m_nextTick && input.tick < *m_nextTick)
        {
            return;
        }

        const auto position = std::lower_bound(m_inputs.begin(), m_inputs.end(), input.tick,
            [](const InputMessage& held, uint64_t tick) { return held.tick < tick; });

        if (position != m_inputs.end() && position->tick == input.tick)
        {
            return;
        }

        m_inputs.insert(position, input);
    }

    std::vector<ConsumedInput> InputBuffer::consume()
    {
        std::vector<ConsumedInput> consumed;

        if (m_inputs.empty())
        {
            if (m_nextTick)
            {
                ++m_counters.starved;
            }

            return consumed;
        }

        if (!m_last)
        {
            m_nextTick = m_inputs.front().tick;
        }

        if (depth() > k_maxDepth)
        {
            const uint64_t keepFrom = m_inputs.back().tick + 1 - k_targetDepth;
            m_counters.skipped += keepFrom - *m_nextTick;
            m_nextTick = keepFrom;
            std::erase_if(m_inputs, [&](const InputMessage& held) { return held.tick < keepFrom; });
        }

        const std::size_t count = depth() > k_catchUpDepth ? k_catchUpInputs : 1;

        for (std::size_t index = 0; index < count && !m_inputs.empty(); ++index)
        {
            if (m_inputs.front().tick == *m_nextTick)
            {
                m_last = m_inputs.front();
                m_inputs.erase(m_inputs.begin());
                consumed.push_back(ConsumedInput{.input = *m_last, .isRepeated = false});
            }
            else
            {
                m_last->tick = *m_nextTick;
                consumed.push_back(ConsumedInput{.input = *m_last, .isRepeated = true});
                ++m_counters.repeated;
            }

            ++*m_nextTick;
        }

        return consumed;
    }

    void InputBuffer::clear()
    {
        m_inputs.clear();
        m_nextTick.reset();
        m_last.reset();
        m_counters = {};
    }

    std::size_t InputBuffer::depth() const
    {
        if (m_inputs.empty() || !m_nextTick)
        {
            return 0;
        }

        return static_cast<std::size_t>(m_inputs.back().tick + 1 - *m_nextTick);
    }

    std::optional<uint64_t> InputBuffer::lastConsumedTick() const
    {
        if (!m_last)
        {
            return std::nullopt;
        }

        return m_last->tick;
    }

    const InputBufferCounters& InputBuffer::counters() const
    {
        return m_counters;
    }
} //namespace lockstep::net
