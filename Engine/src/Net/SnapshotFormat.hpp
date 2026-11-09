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
    // tick:       the host tick it was taken after.
    // worldScale: the World time scale and the tick it took effect.
    // entities:   every replicated entity, sorted by entityKey() of hostEntity.
    struct Snapshot
    {
        uint64_t tick{0};
        ReplicatedTimeScale worldScale{};
        std::vector<EntityState> entities;
    };

    // A client's input of one tick.
    // tick:  the client tick.
    // bytes: the Game-encoded input, at most k_maxInputBytes.
    struct InputMessage
    {
        uint64_t tick{0};
        std::vector<std::byte> bytes;
    };

    // Returns:  the Snapshot message for `snapshot`.
    // snapshot: the state to send.
    [[nodiscard]] std::vector<std::byte> encodeSnapshot(const Snapshot& snapshot);

    // Returns: the snapshot in `message`, or nothing if it is malformed. Entities come back sorted.
    // message: a received message of kind Snapshot.
    [[nodiscard]] std::optional<Snapshot> decodeSnapshot(std::span<const std::byte> message);

    // Returns: the Input message for one tick.
    // tick:    the client tick.
    // input:   the Game-encoded input, at most k_maxInputBytes.
    [[nodiscard]] std::vector<std::byte> encodeInput(uint64_t tick, std::span<const std::byte> input);

    // Returns: the input in `message`, or nothing if it is malformed.
    // message: a received message of kind Input.
    [[nodiscard]] std::optional<InputMessage> decodeInput(std::span<const std::byte> message);
} //namespace lockstep::net

#endif //LOCKSTEP_NET_SNAPSHOT_FORMAT_HPP
