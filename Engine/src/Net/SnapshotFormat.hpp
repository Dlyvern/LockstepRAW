#ifndef LOCKSTEP_NET_SNAPSHOT_FORMAT_HPP
#define LOCKSTEP_NET_SNAPSHOT_FORMAT_HPP

#include "Lockstep/Core/Transform.hpp"
#include "Lockstep/Net/Replication.hpp"
#include "Lockstep/World/Entities.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace lockstep::net
{
    // Returns: a key that orders host entity ids; snapshots keep their entities sorted by it.
    // id:      a host entity id.
    [[nodiscard]] constexpr uint64_t entityKey(world::EntityId id) noexcept
    {
        return (static_cast<uint64_t>(id.index) << 32) | id.generation;
    }

    // One replicated entity in a snapshot.
    struct EntityState
    {
        world::EntityId hostEntity{};
        uint16_t archetype{0};
        world::TimeDomain domain{world::TimeDomain::World};
        world::EntityOwner owner{world::EntityOwner::Host};
        math::Transform transform{};
    };

    // The replicated world state of one host tick.
    // tick:          the host tick it was taken after.
    // worldScale:    the World time scale and the tick it took effect.
    // ackInputTick:  the newest client input tick the host applied up to this tick, if any.
    // clientState:   Game's state for the client's prediction as of this tick, at most
    //                k_maxClientStateBytes.
    // entities:      every replicated entity, sorted by entityKey() of hostEntity.
    struct Snapshot
    {
        uint64_t tick{0};
        ReplicatedTimeScale worldScale{};
        std::optional<uint64_t> ackInputTick;
        std::vector<std::byte> clientState;
        std::vector<EntityState> entities;
    };

    // Most inputs one Input message carries (the newest unacknowledged ones).
    inline constexpr std::size_t k_maxInputsPerMessage{16};

    // A client's input of one tick.
    // tick:     the client tick.
    // viewTick: the host tick (fractional) the client showed when it made the input, if any.
    // bytes:    the Game-encoded input, at most k_maxInputBytes.
    struct InputMessage
    {
        uint64_t tick{0};
        std::optional<double> viewTick;
        std::vector<std::byte> bytes;
    };

    // Returns:  the Snapshot message for `snapshot`.
    // snapshot: the state to send.
    [[nodiscard]] std::vector<std::byte> encodeSnapshot(const Snapshot& snapshot);

    // Returns: the snapshot in `message`, or nothing if it is malformed. Entities come back sorted.
    // message: a received message of kind Snapshot.
    [[nodiscard]] std::optional<Snapshot> decodeSnapshot(std::span<const std::byte> message);

    // Returns: the Input message carrying `inputs`.
    // inputs:  one to k_maxInputsPerMessage inputs, oldest first, with increasing ticks.
    [[nodiscard]] std::vector<std::byte> encodeInputs(std::span<const InputMessage> inputs);

    // Returns: the inputs in `message`, oldest first, or nothing if it is malformed (also when
    //          the ticks do not increase).
    // message: a received message of kind Input.
    [[nodiscard]] std::optional<std::vector<InputMessage>> decodeInputs(std::span<const std::byte> message);
} //namespace lockstep::net

#endif //LOCKSTEP_NET_SNAPSHOT_FORMAT_HPP
