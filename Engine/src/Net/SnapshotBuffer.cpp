#include "SnapshotBuffer.hpp"

#include <algorithm>
#include <cmath>
#include <iterator>
#include <utility>

namespace lockstep::net
{
    namespace
    {
        constexpr std::size_t k_maxSnapshots{64};
        constexpr double k_resyncTicks{20.0};
        constexpr double k_clockCorrection{0.05};

        void blendInto(std::vector<EntityState>& entities, const std::vector<EntityState>& next, float blend)
        {
            auto nextEntity = next.begin();

            for (EntityState& entity : entities)
            {
                const uint64_t key = entityKey(entity.hostEntity);

                while (nextEntity != next.end() && entityKey(nextEntity->hostEntity) < key)
                {
                    ++nextEntity;
                }

                if (nextEntity != next.end() && entityKey(nextEntity->hostEntity) == key)
                {
                    entity.transform = math::interpolate(entity.transform, nextEntity->transform, blend);
                }
            }
        }
    } //namespace

    void SnapshotBuffer::insert(Snapshot snapshot)
    {
        const auto position = std::lower_bound(m_snapshots.begin(), m_snapshots.end(), snapshot.tick,
            [](const Snapshot& held, uint64_t tick) { return held.tick < tick; });

        if (position != m_snapshots.end() && position->tick == snapshot.tick)
        {
            return;
        }

        m_snapshots.insert(position, std::move(snapshot));

        if (m_snapshots.size() > k_maxSnapshots)
        {
            m_snapshots.erase(m_snapshots.begin());
        }
    }

    void SnapshotBuffer::advanceClock(float delayTicks)
    {
        if (m_snapshots.empty())
        {
            return;
        }

        const double newest = static_cast<double>(m_snapshots.back().tick);
        const double target = newest - static_cast<double>(delayTicks);

        if (!m_renderTick || std::abs(target - *m_renderTick) > k_resyncTicks)
        {
            m_renderTick = target;
        }
        else
        {
            const double advanced = *m_renderTick + 1.0;
            m_renderTick = advanced + (target - advanced) * k_clockCorrection;
        }

        m_renderTick = std::min(*m_renderTick, newest);
    }

    std::optional<SampledWorld> SnapshotBuffer::sample()
    {
        if (m_snapshots.empty() || !m_renderTick)
        {
            return std::nullopt;
        }

        const double renderTick = *m_renderTick;
        const auto after = std::find_if(m_snapshots.begin(), m_snapshots.end(),
            [&](const Snapshot& held) { return static_cast<double>(held.tick) > renderTick; });
        const auto before = after == m_snapshots.begin() ? after : std::prev(after);

        SampledWorld world{.worldScale = before->worldScale, .entities = before->entities};

        if (after != m_snapshots.end() && after != before)
        {
            const double span = static_cast<double>(after->tick - before->tick);
            const double blend = std::clamp((renderTick - static_cast<double>(before->tick)) / span, 0.0, 1.0);
            blendInto(world.entities, after->entities, static_cast<float>(blend));
        }

        m_snapshots.erase(m_snapshots.begin(), before);

        return world;
    }

    void SnapshotBuffer::clear()
    {
        m_snapshots.clear();
        m_renderTick.reset();
    }

    std::size_t SnapshotBuffer::size() const
    {
        return m_snapshots.size();
    }

    std::optional<uint64_t> SnapshotBuffer::newestTick() const
    {
        if (m_snapshots.empty())
        {
            return std::nullopt;
        }

        return m_snapshots.back().tick;
    }

    std::optional<double> SnapshotBuffer::renderTick() const
    {
        return m_renderTick;
    }
} //namespace lockstep::net
