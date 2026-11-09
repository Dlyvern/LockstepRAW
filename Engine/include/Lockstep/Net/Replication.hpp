#ifndef LOCKSTEP_NET_REPLICATION_HPP
#define LOCKSTEP_NET_REPLICATION_HPP

#include "Lockstep/Core/Result.hpp"
#include "Lockstep/World/Entities.hpp"
#include "Lockstep/World/TimeDomains.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>

namespace lockstep::net
{
    class Session;
    struct ReplicationState;

    // Most bytes a client's per-tick input message may have.
    inline constexpr std::size_t k_maxInputBytes{64};

    // Most bytes of Game state the host may attach to a snapshot for the client's prediction.
    inline constexpr std::size_t k_maxClientStateBytes{64};

    // How many of its newest unacknowledged inputs a client sends with every input message, so
    // that a lost packet loses no input.
    inline constexpr std::size_t k_inputRedundancy{8};

    // How many ticks behind the newest host snapshot a client renders replicated entities by
    // default (100 ms at 60 Hz): enough to ride out a lost snapshot or two.
    inline constexpr float k_defaultInterpolationDelayTicks{6.0f};

    // Largest interpolation delay accepted by Replication::setInterpolationDelay(), in ticks.
    inline constexpr float k_maxInterpolationDelayTicks{60.0f};

    // The World time domain's scale as the host decided it.
    // scale: the World domain's time scale.
    // tick:  the first host tick simulated with `scale`.
    struct ReplicatedTimeScale
    {
        float scale{1.0f};
        uint64_t tick{0};
    };

    // An entity a client mirrors from the host.
    // entity:     the client's local copy.
    // hostEntity: the entity's id on the host; serves as its network id.
    // archetype:  Game's tag for what the entity is (see Replication::setArchetype()).
    struct MirroredEntity
    {
        world::EntityId entity{};
        world::EntityId hostEntity{};
        uint16_t archetype{0};
    };

    // One client input the host applies this tick.
    // tick:       the client's tick the input belongs to.
    // viewTick:   the host tick (fractional) the client was showing its interpolated entities at
    //             when it made the input, for lag compensation; nothing before it showed any.
    // bytes:      the input as Game encoded it.
    // isRepeated: the input of `tick` was lost; the previous input stands in for it.
    struct RemoteInput
    {
        uint64_t tick{0};
        std::optional<double> viewTick;
        std::span<const std::byte> bytes;
        bool isRepeated{false};
    };

    // What the newest snapshot told the client about its own inputs.
    // hostTick:    the host tick of the snapshot.
    // inputTick:   the newest client input tick the host had applied by then; nothing if none.
    // clientState: the Game state the host attached for the client's prediction, as of
    //              `hostTick` (see Replication::setClientState()); empty if none.
    struct InputAck
    {
        uint64_t hostTick{0};
        std::optional<uint64_t> inputTick;
        std::span<const std::byte> clientState;
    };

    // Replication counters and clocks, for the dev tools.
    // snapshotsSent:      snapshots the host sent on the current connection.
    // snapshotsReceived:  snapshots the client received on the current connection.
    // inputsSent:         input messages the client sent on the current connection.
    // inputsReceived:     input messages the host received on the current connection.
    // bufferedSnapshots:  snapshots the client holds for interpolation.
    // newestSnapshotTick: host tick of the newest snapshot the client holds.
    // renderTick:         host tick (fractional) the client currently shows.
    // mirroredEntities:   entities the client mirrors.
    // ackedInputTick:     host: newest client input tick applied; client: newest one the host
    //                     acknowledged.
    // inputBufferDepth:   host: client ticks buffered ahead; client: inputs sent but not yet
    //                     acknowledged.
    // inputsRepeated:     host: lost inputs replaced by the previous one.
    // inputsStarved:      host: ticks the client's input had not arrived yet (its pawn waited).
    // inputsSkipped:      host: inputs dropped because the buffer held too many.
    // viewLagTicks:       host: how many ticks behind the host the client's view was for its
    //                     newest input (what lag compensation rewinds).
    // corrections:        client: prediction corrections Game recorded.
    // lastCorrection:     client: size of the newest correction, in metres.
    // largestCorrection:  client: largest correction, in metres.
    struct ReplicationStats
    {
        uint64_t snapshotsSent{0};
        uint64_t snapshotsReceived{0};
        uint64_t inputsSent{0};
        uint64_t inputsReceived{0};
        std::size_t bufferedSnapshots{0};
        std::optional<uint64_t> newestSnapshotTick;
        std::optional<double> renderTick;
        std::size_t mirroredEntities{0};
        std::optional<uint64_t> ackedInputTick;
        std::size_t inputBufferDepth{0};
        uint64_t inputsRepeated{0};
        uint64_t inputsStarved{0};
        uint64_t inputsSkipped{0};
        std::optional<double> viewLagTicks;
        uint64_t corrections{0};
        float lastCorrection{0.0f};
        float largestCorrection{0.0f};
    };

    // Host-authoritative replication of World entities and the World time scale over a Session,
    // plus the client's per-tick inputs to the host and the hooks for client-side prediction.
    //
    // Host: after every tick it sends a full snapshot (unreliable) of every entity that is not
    // client-only: host EntityId, archetype, time domain, owner and transform, plus the World
    // time scale and the tick it took effect. Every snapshot is complete, so a lost one costs
    // nothing and spawns and despawns need no separate messages: an entity is spawned when it
    // first appears and despawned when it is missing.
    //
    // Client: buffers the snapshots and each tick samples them at a render point a few ticks
    // behind the newest one (the interpolation delay), blending the two snapshots around it.
    // The sample drives local mirror entities: created, moved (as their current transform, so
    // FrameInfo::alpha blending stays smooth) and destroyed to match the host. It also sets the
    // World time scale, so on a connected client Game must not set it. Entities the client
    // created itself are never touched; nothing is sent upstream except inputs.
    // When the connection ends, the mirror entities are destroyed.
    //
    // Inputs: the client stamps each tick's input with its tick and the host tick it shows, and
    // sends it together with its newest unacknowledged inputs (up to k_inputRedundancy), so a
    // lost packet loses nothing. The host puts them back in order in a small buffer and hands
    // Game one per tick through remoteInputs(): none while the next one is late (the pawn
    // waits), the previous one in place of a lost one, two per tick while the buffer is too
    // deep. Each snapshot acknowledges the newest input applied and carries Game's state for the
    // client's prediction (setClientState()), which the client reads through inputAck() to
    // reconcile its prediction.
    //
    // The run loop calls sendTick() after GameApi::fixedTick and receiveTick() after
    // Session::pump(); Game does not call them.
    class Replication
    {
    public:
        // Creates the replication for one session.
        // session:     the session to replicate over.
        // entities:    the entity store whose entities the host sends and the client mirrors.
        // timeDomains: the time domains whose World scale is replicated.
        // All three must outlive the Replication.
        Replication(Session& session, world::Entities& entities, world::TimeDomains& timeDomains);

        ~Replication();

        Replication(const Replication&) = delete;
        Replication& operator=(const Replication&) = delete;
        Replication(Replication&&) = delete;
        Replication& operator=(Replication&&) = delete;

        // Host: tags an entity so the client can tell what it is (which mesh to draw, ...).
        // Untagged entities have archetype 0. Game decides what the numbers mean.
        // entity:    a living entity on the host.
        // archetype: the tag.
        // Returns:   success, or an error if `entity` is not alive.
        [[nodiscard]] Status setArchetype(world::EntityId entity, uint16_t archetype);

        // Client: sets this tick's input. Call during GameApi::fixedTick; it is sent stamped with
        // the tick and the shown host tick when the tick ends. Ignored unless connected as a
        // client.
        // input:   the input as Game encodes it, at most k_maxInputBytes.
        // Returns: success, or an error if `input` is too large.
        [[nodiscard]] Status setLocalInput(std::span<const std::byte> input);

        // Host: the client inputs to apply this tick, oldest first, each client tick exactly
        // once. Game must apply all of them during this fixedTick: the next snapshot
        // acknowledges the last one as applied.
        // Returns: usually one input, none while the next is late, two while catching up. Valid
        //          until the next receiveTick().
        [[nodiscard]] std::span<const RemoteInput> remoteInputs() const;

        // Host: sets Game's state of the client's predicted objects (such as its pawn's position
        // and speed) after this tick's inputs, sent with this tick's snapshot. Call during
        // GameApi::fixedTick, after applying remoteInputs(). Ignored unless connected as a host.
        // state:   the state as Game encodes it, at most k_maxClientStateBytes.
        // Returns: success, or an error if `state` is too large.
        [[nodiscard]] Status setClientState(std::span<const std::byte> state);

        // Client: the acknowledgement of the newest snapshot that arrived in the last
        // receiveTick(), if it is newer than every snapshot before. Read it in fixedTick to
        // reconcile the prediction.
        // Returns: the acknowledgement, valid until the next receiveTick(), or nothing.
        [[nodiscard]] std::optional<InputAck> inputAck() const;

        // Client: records a prediction correction for the dev tools.
        // distance: how far the prediction was off, in metres.
        void recordCorrection(float distance);

        // Client: the entities mirrored from the host.
        // Returns: one entry per mirrored entity, valid until the next receiveTick().
        [[nodiscard]] std::span<const MirroredEntity> mirrored() const;

        // Returns: the World time scale the host decided, as of the tick this machine shows: on
        //          the host its own scale, on a client the scale of the sampled snapshot.
        [[nodiscard]] ReplicatedTimeScale worldTimeScale() const;

        // Client: sets how far behind the newest snapshot replicated entities are shown.
        // ticks: the delay in ticks, clamped to [0, k_maxInterpolationDelayTicks]. Larger hides
        //        more loss and jitter, smaller shows the host's state sooner.
        void setInterpolationDelay(float ticks);

        // Returns: the interpolation delay in ticks.
        [[nodiscard]] float interpolationDelay() const;

        // Returns: counters and clocks for the dev tools.
        [[nodiscard]] ReplicationStats stats() const;

        // Host: sends the snapshot of the tick that just ran. Client: sends the input message.
        // Called by the run loop after GameApi::fixedTick and before Session::pump().
        // tick: the tick that just ran.
        void sendTick(uint64_t tick);

        // Client: takes in the snapshots that arrived and updates the mirror entities and the
        // World scale. Host: takes in the client's input. Called by the run loop after
        // Session::pump().
        void receiveTick();

    private:
        std::unique_ptr<ReplicationState> m_state;
    };
} //namespace lockstep::net

#endif //LOCKSTEP_NET_REPLICATION_HPP
