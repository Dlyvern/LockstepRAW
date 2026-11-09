#ifndef LOCKSTEP_NET_REPLICATION_STATE_HPP
#define LOCKSTEP_NET_REPLICATION_STATE_HPP

#include "Lockstep/Core/Result.hpp"
#include "Lockstep/Net/Replication.hpp"
#include "Lockstep/Net/Session.hpp"

#include "InputBuffer.hpp"
#include "SnapshotBuffer.hpp"
#include "SnapshotFormat.hpp"

#include <cstddef>
#include <cstdint>
#include <deque>
#include <optional>
#include <string>
#include <vector>

namespace lockstep::net
{
    // Internal state of a Replication.
    // localInput:      client: this tick's input, set by Game.
    // pendingInputs:   client: inputs sent but not acknowledged yet, oldest first.
    // ack:             client: the newest acknowledgement received.
    // hasNewAck:       client: `ack` arrived in the last receiveTick().
    // inputBuffer:     host: the client's inputs, in order.
    // consumedInputs:  host: the inputs handed to Game for this tick; remoteInputs views them.
    // clientState:     host: Game's prediction state for this tick's snapshot.
    // lastTick:        the tick of the last sendTick().
    struct ReplicationState
    {
        struct ArchetypeSlot
        {
            world::EntityId entity{};
            uint16_t archetype{0};
        };

        struct Acknowledgement
        {
            uint64_t hostTick{0};
            std::optional<uint64_t> inputTick;
            std::vector<std::byte> clientState;
        };

        ReplicationState(Session& session, world::Entities& entities, world::TimeDomains& timeDomains)
            : session(session)
            , entities(entities)
            , timeDomains(timeDomains)
            , worldScale{.scale = timeDomains.scale(world::TimeDomain::World), .tick = 0}
        {
        }

        Session& session;
        world::Entities& entities;
        world::TimeDomains& timeDomains;
        ReplicatedTimeScale worldScale{};
        float interpolationDelay{k_defaultInterpolationDelayTicks};
        std::vector<ArchetypeSlot> archetypes;
        std::optional<std::vector<std::byte>> localInput;
        std::deque<InputMessage> pendingInputs;
        std::optional<Acknowledgement> ack;
        bool hasNewAck{false};
        InputBuffer inputBuffer;
        std::vector<ConsumedInput> consumedInputs;
        std::vector<RemoteInput> remoteInputs;
        std::vector<std::byte> clientState;
        uint64_t lastTick{0};
        SnapshotBuffer snapshots;
        std::vector<MirroredEntity> mirrored;
        ReplicationStats counters{};
        bool sendFailureReported{false};
        bool spawnFailureReported{false};
    };

    // Logs a replication event with the session's role.
    // state:   the replication.
    // message: one line.
    void log(const ReplicationState& state, const std::string& message);

    // Logs a failed send once until a send succeeds again; failures the Session already
    // reported are not repeated.
    // state: the replication.
    // sent:  the send result.
    // what:  what was sent, for the log.
    void reportSend(ReplicationState& state, const Status& sent, const char* what);

    // Client: queues this tick's input with the pending ones and sends the newest of them.
    // state: the replication.
    // tick:  the tick that just ran.
    void sendInputs(ReplicationState& state, uint64_t tick);

    // Host: buffers the Input messages of the last pump and takes this tick's inputs from the
    // buffer for Game.
    // state: the replication.
    void receiveInputs(ReplicationState& state);

    // Client: takes the acknowledgement of a received snapshot if it is the newest, and drops
    // the pending inputs it covers.
    // state:    the replication.
    // snapshot: a received snapshot.
    void receiveAcknowledgement(ReplicationState& state, const Snapshot& snapshot);

    // Forgets every input, acknowledgement and prediction counter (for a new connection).
    // state: the replication.
    void clearInputs(ReplicationState& state);
} //namespace lockstep::net

#endif //LOCKSTEP_NET_REPLICATION_STATE_HPP
