#include "WireFormat.hpp"

#include <algorithm>
#include <bit>
#include <utility>

namespace lockstep::net
{
    namespace
    {
        constexpr unsigned k_bitsPerByte{8};
        constexpr uint64_t k_byteMask{0xFFu};
    } //namespace

    std::optional<MessageKind> messageKind(std::span<const std::byte> message)
    {
        if (message.empty())
        {
            return std::nullopt;
        }

        const auto kind = static_cast<MessageKind>(message.front());

        if (kind != MessageKind::Heartbeat && kind != MessageKind::Game && kind != MessageKind::Snapshot
            && kind != MessageKind::Input)
        {
            return std::nullopt;
        }

        return kind;
    }

    std::array<std::byte, k_heartbeatBytes> encodeHeartbeat(uint64_t tick)
    {
        std::array<std::byte, k_heartbeatBytes> message{};
        message[0] = static_cast<std::byte>(MessageKind::Heartbeat);

        for (size_t index = 0; index < sizeof(uint64_t); ++index)
        {
            message[k_kindBytes + index] = static_cast<std::byte>((tick >> (index * k_bitsPerByte)) & k_byteMask);
        }

        return message;
    }

    std::optional<uint64_t> decodeHeartbeat(std::span<const std::byte> message)
    {
        if (message.size() != k_heartbeatBytes || messageKind(message) != MessageKind::Heartbeat)
        {
            return std::nullopt;
        }

        uint64_t tick = 0;

        for (size_t index = 0; index < sizeof(uint64_t); ++index)
        {
            tick |= static_cast<uint64_t>(message[k_kindBytes + index]) << (index * k_bitsPerByte);
        }

        return tick;
    }

    std::vector<std::byte> encodeGameMessage(std::span<const std::byte> payload)
    {
        std::vector<std::byte> message(k_kindBytes + payload.size());
        message[0] = static_cast<std::byte>(MessageKind::Game);
        std::copy(payload.begin(), payload.end(), message.begin() + k_kindBytes);

        return message;
    }

    std::span<const std::byte> gamePayload(std::span<const std::byte> message)
    {
        return message.subspan(k_kindBytes);
    }

    ByteWriter::ByteWriter(MessageKind kind)
    {
        m_bytes.push_back(static_cast<std::byte>(kind));
    }

    void ByteWriter::writeU8(uint8_t value)
    {
        m_bytes.push_back(static_cast<std::byte>(value));
    }

    void ByteWriter::writeU16(uint16_t value)
    {
        writeLittleEndian(value, sizeof(value));
    }

    void ByteWriter::writeU32(uint32_t value)
    {
        writeLittleEndian(value, sizeof(value));
    }

    void ByteWriter::writeU64(uint64_t value)
    {
        writeLittleEndian(value, sizeof(value));
    }

    void ByteWriter::writeF32(float value)
    {
        writeU32(std::bit_cast<uint32_t>(value));
    }

    void ByteWriter::writeBytes(std::span<const std::byte> bytes)
    {
        m_bytes.insert(m_bytes.end(), bytes.begin(), bytes.end());
    }

    std::vector<std::byte> ByteWriter::take()
    {
        return std::move(m_bytes);
    }

    void ByteWriter::writeLittleEndian(uint64_t value, size_t byteCount)
    {
        for (size_t index = 0; index < byteCount; ++index)
        {
            m_bytes.push_back(static_cast<std::byte>((value >> (index * k_bitsPerByte)) & k_byteMask));
        }
    }

    ByteReader::ByteReader(std::span<const std::byte> message)
        : m_bytes(message)
        , m_offset(k_kindBytes)
        , m_isValid(message.size() >= k_kindBytes)
    {
    }

    uint8_t ByteReader::readU8()
    {
        return static_cast<uint8_t>(readLittleEndian(sizeof(uint8_t)));
    }

    uint16_t ByteReader::readU16()
    {
        return static_cast<uint16_t>(readLittleEndian(sizeof(uint16_t)));
    }

    uint32_t ByteReader::readU32()
    {
        return static_cast<uint32_t>(readLittleEndian(sizeof(uint32_t)));
    }

    uint64_t ByteReader::readU64()
    {
        return readLittleEndian(sizeof(uint64_t));
    }

    float ByteReader::readF32()
    {
        return std::bit_cast<float>(readU32());
    }

    std::span<const std::byte> ByteReader::readBytes(size_t count)
    {
        if (!m_isValid || remaining() < count)
        {
            m_isValid = false;
            return {};
        }

        const std::span<const std::byte> bytes = m_bytes.subspan(m_offset, count);
        m_offset += count;

        return bytes;
    }

    size_t ByteReader::remaining() const
    {
        return m_offset <= m_bytes.size() ? m_bytes.size() - m_offset : 0;
    }

    bool ByteReader::isValid() const
    {
        return m_isValid;
    }

    uint64_t ByteReader::readLittleEndian(size_t byteCount)
    {
        const std::span<const std::byte> bytes = readBytes(byteCount);
        uint64_t value = 0;

        for (size_t index = 0; index < bytes.size(); ++index)
        {
            value |= static_cast<uint64_t>(bytes[index]) << (index * k_bitsPerByte);
        }

        return value;
    }
} //namespace lockstep::net
