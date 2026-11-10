#include "Gore/GoreEvents.hpp"

#include "Player/WireBytes.hpp"

#include <algorithm>

namespace game
{
    namespace
    {
        constexpr std::byte k_goreTag{0x47};
        constexpr std::size_t k_headerBytes{2};
        constexpr std::byte k_corpseHitTag{0x43};
        constexpr std::size_t k_eventBytes{1 + 8 + 1 + 12 + 12 + 4 + 1 + 1 + 1};
        constexpr std::size_t k_corpseHitBytes{1 + 8 + 1 + 1 + 12 + 12};
        constexpr std::size_t k_maxCount{255};

        void writeUint32(std::span<std::byte> bytes, std::size_t offset, uint32_t value)
        {
            for (std::size_t index = 0; index < sizeof(uint32_t); ++index)
            {
                bytes[offset + index] = static_cast<std::byte>((value >> (8 * index)) & 0xFFu);
            }
        }

        uint32_t readUint32(std::span<const std::byte> bytes, std::size_t offset)
        {
            uint32_t value = 0;

            for (std::size_t index = 0; index < sizeof(uint32_t); ++index)
            {
                value |= static_cast<uint32_t>(bytes[offset + index]) << (8 * index);
            }

            return value;
        }

        void writeVec3(std::span<std::byte> bytes, std::size_t offset, const math::Vec3& value)
        {
            writeFloat(bytes, offset, value.x);
            writeFloat(bytes, offset + 4, value.y);
            writeFloat(bytes, offset + 8, value.z);
        }

        math::Vec3 readVec3(std::span<const std::byte> bytes, std::size_t offset)
        {
            return math::Vec3{readFloat(bytes, offset), readFloat(bytes, offset + 4), readFloat(bytes, offset + 8)};
        }

        bool isValidLimb(uint8_t limb)
        {
            return limb < lockstep::anim::k_limbCount || limb == static_cast<uint8_t>(lockstep::anim::Limb::None);
        }
    } //namespace

    std::vector<std::byte> encodeGoreEvents(std::span<const GoreEvent> events)
    {
        const std::size_t count = std::min(events.size(), k_maxCount);
        std::vector<std::byte> bytes(k_headerBytes + count * k_eventBytes);
        bytes[0] = k_goreTag;
        bytes[1] = static_cast<std::byte>(count);

        for (std::size_t index = 0; index < count; ++index)
        {
            const GoreEvent& event = events[index];
            const std::size_t offset = k_headerBytes + index * k_eventBytes;
            bytes[offset] = static_cast<std::byte>(event.kind);
            writeUint32(bytes, offset + 1, event.target.index);
            writeUint32(bytes, offset + 5, event.target.generation);
            bytes[offset + 9] = static_cast<std::byte>(event.limb);
            writeVec3(bytes, offset + 10, event.point);
            writeVec3(bytes, offset + 22, event.direction);
            writeUint32(bytes, offset + 34, event.seed);
            bytes[offset + 38] = static_cast<std::byte>(event.body);
            bytes[offset + 39] = static_cast<std::byte>(event.shooter);
            bytes[offset + 40] = static_cast<std::byte>(event.damage);
        }

        return bytes;
    }

    std::optional<std::vector<GoreEvent>> decodeGoreEvents(std::span<const std::byte> bytes)
    {
        if (bytes.size() < k_headerBytes || bytes[0] != k_goreTag)
        {
            return std::nullopt;
        }

        const auto count = static_cast<std::size_t>(bytes[1]);

        if (bytes.size() != k_headerBytes + count * k_eventBytes)
        {
            return std::nullopt;
        }

        std::vector<GoreEvent> events;

        for (std::size_t index = 0; index < count; ++index)
        {
            const std::size_t offset = k_headerBytes + index * k_eventBytes;
            const auto kind = static_cast<uint8_t>(bytes[offset]);
            const auto limb = static_cast<uint8_t>(bytes[offset + 9]);

            if (kind > static_cast<uint8_t>(GoreEventKind::HeadBurst) || !isValidLimb(limb))
            {
                return std::nullopt;
            }

            events.push_back(GoreEvent{.kind = static_cast<GoreEventKind>(kind),
                .target = lockstep::world::EntityId{.index = readUint32(bytes, offset + 1),
                    .generation = readUint32(bytes, offset + 5)},
                .limb = static_cast<lockstep::anim::Limb>(limb),
                .point = readVec3(bytes, offset + 10),
                .direction = readVec3(bytes, offset + 22),
                .seed = readUint32(bytes, offset + 34),
                .body = static_cast<uint8_t>(bytes[offset + 38]),
                .shooter = static_cast<uint8_t>(bytes[offset + 39]),
                .damage = static_cast<uint8_t>(bytes[offset + 40])});
        }

        return events;
    }

    std::vector<std::byte> encodeCorpseHit(const CorpseHitReport& report)
    {
        std::vector<std::byte> bytes(k_corpseHitBytes);
        bytes[0] = k_corpseHitTag;
        writeUint32(bytes, 1, report.target.index);
        writeUint32(bytes, 5, report.target.generation);
        bytes[9] = static_cast<std::byte>(report.body);
        bytes[10] = static_cast<std::byte>(report.limb);
        writeVec3(bytes, 11, report.point);
        writeVec3(bytes, 23, report.direction);
        return bytes;
    }

    std::optional<CorpseHitReport> decodeCorpseHit(std::span<const std::byte> bytes)
    {
        if (bytes.size() != k_corpseHitBytes || bytes[0] != k_corpseHitTag
            || !isValidLimb(static_cast<uint8_t>(bytes[10])))
        {
            return std::nullopt;
        }

        return CorpseHitReport{.target = lockstep::world::EntityId{.index = readUint32(bytes, 1),
                .generation = readUint32(bytes, 5)},
            .body = static_cast<uint8_t>(bytes[9]),
            .limb = static_cast<lockstep::anim::Limb>(bytes[10]),
            .point = readVec3(bytes, 11),
            .direction = readVec3(bytes, 23)};
    }
} //namespace game
