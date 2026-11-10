#include "Npcs/NpcMessages.hpp"

#include <algorithm>

namespace game
{
    namespace
    {
        constexpr std::byte k_stateTag{0x4E};
        constexpr std::byte k_calloutTag{0x4F};
        constexpr std::byte k_deadBit{0x01};
        constexpr std::byte k_alertBit{0x02};
        constexpr unsigned k_gearLookShift{2};
        constexpr std::byte k_gearLookBits{0x03};
        constexpr std::size_t k_headerBytes{2};
        constexpr std::size_t k_entityBytes{8};
        constexpr std::size_t k_viewBytes{k_entityBytes + 2};
        constexpr std::size_t k_maxCount{255};

        void writeUint32(std::vector<std::byte>& bytes, uint32_t value)
        {
            for (std::size_t index = 0; index < sizeof(uint32_t); ++index)
            {
                bytes.push_back(static_cast<std::byte>((value >> (8 * index)) & 0xFFu));
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

        void writeEntity(std::vector<std::byte>& bytes, lockstep::world::EntityId entity)
        {
            writeUint32(bytes, entity.index);
            writeUint32(bytes, entity.generation);
        }

        GearLook gearLookOf(std::byte bits)
        {
            const auto value = static_cast<uint8_t>(bits);
            return value < k_gearLookCount ? static_cast<GearLook>(value) : GearLook::Full;
        }

        lockstep::world::EntityId readEntity(std::span<const std::byte> bytes, std::size_t offset)
        {
            return lockstep::world::EntityId{.index = readUint32(bytes, offset),
                .generation = readUint32(bytes, offset + sizeof(uint32_t))};
        }
    } //namespace

    NpcView npcView(const Npc& npc)
    {
        return NpcView{.entity = npc.entity,
            .isDead = npc.mode == NpcMode::Dead,
            .isAlert = npc.mode == NpcMode::Alert || npc.mode == NpcMode::Combat,
            .shotCount = npc.shotCount,
            .gearLook = npc.faction == NpcFaction::Squad ? squadGearLook(npc.name) : mercGearLook(npc.name)};
    }

    std::vector<std::byte> encodeNpcState(const NpcStateMessage& message)
    {
        const std::size_t count = std::min(message.npcs.size(), k_maxCount);
        std::vector<std::byte> bytes{k_stateTag, static_cast<std::byte>(count)};
        bytes.reserve(k_headerBytes + count * k_viewBytes);

        for (std::size_t index = 0; index < count; ++index)
        {
            const NpcView& view = message.npcs[index];
            writeEntity(bytes, view.entity);
            const std::byte gearLook = (static_cast<std::byte>(view.gearLook) & k_gearLookBits) << k_gearLookShift;
            bytes.push_back((view.isDead ? k_deadBit : std::byte{0}) | (view.isAlert ? k_alertBit : std::byte{0})
                | gearLook);
            bytes.push_back(static_cast<std::byte>(view.shotCount));
        }

        return bytes;
    }

    std::optional<NpcStateMessage> decodeNpcState(std::span<const std::byte> bytes)
    {
        if (bytes.size() < k_headerBytes || bytes[0] != k_stateTag)
        {
            return std::nullopt;
        }

        const auto count = static_cast<std::size_t>(bytes[1]);

        if (bytes.size() != k_headerBytes + count * k_viewBytes)
        {
            return std::nullopt;
        }

        NpcStateMessage message{};

        for (std::size_t index = 0; index < count; ++index)
        {
            const std::size_t offset = k_headerBytes + index * k_viewBytes;
            const std::byte flags = bytes[offset + k_entityBytes];
            message.npcs.push_back(NpcView{.entity = readEntity(bytes, offset),
                .isDead = (flags & k_deadBit) != std::byte{0},
                .isAlert = (flags & k_alertBit) != std::byte{0},
                .shotCount = static_cast<uint8_t>(bytes[offset + k_entityBytes + 1]),
                .gearLook = gearLookOf((flags >> k_gearLookShift) & k_gearLookBits)});
        }

        return message;
    }

    std::vector<std::byte> encodeNpcCallout(const NpcCalloutMessage& message)
    {
        const std::size_t length = std::min(message.id.size(), k_maxCount);
        std::vector<std::byte> bytes{k_calloutTag};
        writeEntity(bytes, message.speaker);
        bytes.push_back(static_cast<std::byte>(length));

        for (std::size_t index = 0; index < length; ++index)
        {
            bytes.push_back(static_cast<std::byte>(message.id[index]));
        }

        return bytes;
    }

    std::optional<NpcCalloutMessage> decodeNpcCallout(std::span<const std::byte> bytes)
    {
        constexpr std::size_t k_calloutHeader{1 + k_entityBytes + 1};

        if (bytes.size() < k_calloutHeader || bytes[0] != k_calloutTag
            || bytes.size() != k_calloutHeader + static_cast<std::size_t>(bytes[1 + k_entityBytes]))
        {
            return std::nullopt;
        }

        NpcCalloutMessage message{.speaker = readEntity(bytes, 1)};

        for (const std::byte byte : bytes.subspan(k_calloutHeader))
        {
            message.id.push_back(static_cast<char>(byte));
        }

        return message;
    }
} //namespace game
