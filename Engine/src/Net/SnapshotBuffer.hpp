#ifndef LOCKSTEP_NET_SNAPSHOT_BUFFER_HPP
#define LOCKSTEP_NET_SNAPSHOT_BUFFER_HPP

#include "SnapshotFormat.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace lockstep::net
{
    // What a client shows at its render point.
    // worldScale: the World time scale of the snapshot at or before the render point.
    // entities:   the entities of that snapshot, blended toward the next snapshot; sorted by
    //             entityKey() of hostEntity.
    struct SampledWorld
    {
        ReplicatedTimeScale worldScale{};
        std::vector<EntityState> entities;
    };

    // A client's received snapshots, ordered by tick, and its render clock: the fractional host
    // tick it shows, advanced one tick per client tick and steered gently toward a fixed delay
    // behind the newest snapshot (snapping when far off, e.g. right after connecting).
    class SnapshotBuffer
    {
    public:
        // Adds a received snapshot. Duplicates are ignored; when full, the oldest is dropped.
        // snapshot: the snapshot.
        void insert(Snapshot snapshot);

        // Advances the render clock by one tick. Does nothing while the buffer is empty.
        // delayTicks: how far behind the newest snapshot the clock should run.
        void advanceClock(float delayTicks);

        // Samples the snapshots at the render clock and drops the ones before the snapshot at
        // or before it. Never extrapolates: past the newest snapshot it holds the newest.
        // Returns: the sampled world, or nothing while the buffer is empty or the clock has not
        //          started.
        [[nodiscard]] std::optional<SampledWorld> sample();

        // Drops every snapshot and stops the render clock.
        void clear();

        // Returns: the number of snapshots held.
        [[nodiscard]] std::size_t size() const;

        // Returns: the tick of the newest snapshot held, or nothing.
        [[nodiscard]] std::optional<uint64_t> newestTick() const;

        // Returns: the render clock, or nothing before it started.
        [[nodiscard]] std::optional<double> renderTick() const;

    private:
        std::vector<Snapshot> m_snapshots;
        std::optional<double> m_renderTick;
    };
} //namespace lockstep::net

#endif //LOCKSTEP_NET_SNAPSHOT_BUFFER_HPP
