#ifndef LOCKSTEP_NET_WIRE_FORMAT_HPP
#define LOCKSTEP_NET_WIRE_FORMAT_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace lockstep::net
{
    // First byte of every message on the wire: who the message is for.
    // Heartbeat: Engine's per-tick heartbeat, followed by the tick as 8 little-endian bytes.
    // Game:      a Game message, followed by its payload.
    // Snapshot:  host to client, the replicated world state of one tick (SnapshotFormat.hpp).
    // Input:     client to host, the client's input of one tick (SnapshotFormat.hpp).
    enum class MessageKind : uint8_t
    {
        Heartbeat = 0,
        Game = 1,
        Snapshot = 2,
        Input = 3
    };

    // Size of the MessageKind prefix.
    inline constexpr size_t k_kindBytes{1};

    // Size of a whole heartbeat message.
    inline constexpr size_t k_heartbeatBytes{k_kindBytes + sizeof(uint64_t)};

    // Returns: the kind of `message`, or nothing if it is empty or of an unknown kind.
    // message: a received message.
    [[nodiscard]] std::optional<MessageKind> messageKind(std::span<const std::byte> message);

    // Returns: a heartbeat message carrying `tick`.
    // tick:    the sender's simulation tick.
    [[nodiscard]] std::array<std::byte, k_heartbeatBytes> encodeHeartbeat(uint64_t tick);

    // Returns: the tick in a heartbeat message, or nothing if `message` is not a valid heartbeat.
    // message: a received message.
    [[nodiscard]] std::optional<uint64_t> decodeHeartbeat(std::span<const std::byte> message);

    // Returns: a Game message wrapping `payload`.
    // payload: the Game bytes.
    [[nodiscard]] std::vector<std::byte> encodeGameMessage(std::span<const std::byte> payload);

    // Returns: the Game payload of `message`, which must be of kind Game.
    // message: a received Game message.
    [[nodiscard]] std::span<const std::byte> gamePayload(std::span<const std::byte> message);

    // Builds a message: the kind byte, then little-endian values in the order written.
    class ByteWriter
    {
    public:
        // Starts a message.
        // kind: the message kind, written as the first byte.
        explicit ByteWriter(MessageKind kind);

        // Appends one byte.
        // value: the byte.
        void writeU8(uint8_t value);

        // Appends a 16-bit value.
        // value: the value.
        void writeU16(uint16_t value);

        // Appends a 32-bit value.
        // value: the value.
        void writeU32(uint32_t value);

        // Appends a 64-bit value.
        // value: the value.
        void writeU64(uint64_t value);

        // Appends a float by its IEEE-754 bits.
        // value: the value.
        void writeF32(float value);

        // Appends raw bytes.
        // bytes: the bytes.
        void writeBytes(std::span<const std::byte> bytes);

        // Returns: the finished message. The writer is empty afterwards.
        [[nodiscard]] std::vector<std::byte> take();

    private:
        void writeLittleEndian(uint64_t value, size_t byteCount);

        std::vector<std::byte> m_bytes;
    };

    // Reads the values of a message written by ByteWriter, after its kind byte. Reading past the
    // end yields zeros and marks the reader as failed; check isValid() after decoding.
    class ByteReader
    {
    public:
        // Starts reading a message after its kind byte.
        // message: the whole message, at least one byte long.
        explicit ByteReader(std::span<const std::byte> message);

        // Returns: the next byte, or 0 past the end.
        [[nodiscard]] uint8_t readU8();

        // Returns: the next 16-bit value, or 0 past the end.
        [[nodiscard]] uint16_t readU16();

        // Returns: the next 32-bit value, or 0 past the end.
        [[nodiscard]] uint32_t readU32();

        // Returns: the next 64-bit value, or 0 past the end.
        [[nodiscard]] uint64_t readU64();

        // Returns: the next float, or 0 past the end.
        [[nodiscard]] float readF32();

        // Returns: the next `count` bytes, or an empty span past the end.
        // count:   number of bytes to read.
        [[nodiscard]] std::span<const std::byte> readBytes(size_t count);

        // Returns: the number of bytes not read yet.
        [[nodiscard]] size_t remaining() const;

        // Returns: true if every read so far was within the message.
        [[nodiscard]] bool isValid() const;

    private:
        [[nodiscard]] uint64_t readLittleEndian(size_t byteCount);

        std::span<const std::byte> m_bytes;
        size_t m_offset{0};
        bool m_isValid{true};
    };
} //namespace lockstep::net

#endif //LOCKSTEP_NET_WIRE_FORMAT_HPP
