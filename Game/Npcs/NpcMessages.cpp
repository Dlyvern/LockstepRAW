#include "Npcs/NpcMessages.hpp"

#include <algorithm>
#include <cmath>

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
        constexpr std::byte k_hitBit{0x10};
        constexpr unsigned k_bodyShift{5};
        constexpr std::byte k_bodyBits{0x03};
        constexpr float k_headingSteps{256.0f};
        constexpr std::size_t k_headerBytes{2};
        constexpr std::size_t k_entityBytes{8};
        constexpr std::size_t k_viewBytes{k_entityBytes + 4};
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

        NpcBody bodyOf(std::byte bits)
        {
            const auto value = static_cast<uint8_t>(bits);
            return value < k_npcBodyCount ? static_cast<NpcBody>(value) : NpcBody::Swat;
        }

        std::optional<uint8_t> headingStep(const std::optional<float>& yaw)
        {
            if (!yaw)
            {
                return std::nullopt;
            }

            const float turns = *yaw / (2.0f * math::k_pi);
            const float step = std::round((turns - std::floor(turns)) * k_headingSteps);
            return static_cast<uint8_t>(static_cast<int>(step) & 0xFF);
        }

        lockstep::anim::Limb limbOf(std::byte value)
        {
            const auto limb = static_cast<uint8_t>(value);
            return limb < lockstep::anim::k_limbCount ? static_cast<lockstep::anim::Limb>(limb) : lockstep::anim::Limb::None;
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
            .gearLook = npc.faction == NpcFaction::Squad ? squadGearLook(npc.name) : mercGearLook(npc.name),
            .body = npc.faction == NpcFaction::Squad ? squadBody(npc.name) : mercBody(),
            .hitHeading = npc.mode == NpcMode::Dead ? headingStep(npc.hitYaw) : std::nullopt,
            .hitLimb = npc.mode == NpcMode::Dead ? npc.hitLimb : lockstep::anim::Limb::None};
    }

    std::optional<math::Vec3> hitDirection(const NpcView& view)
    {
        if (!view.hitHeading)
        {
            return std::nullopt;
        }

        const float yaw = static_cast<float>(*view.hitHeading) / k_headingSteps * 2.0f * math::k_pi;
        return math::Vec3{std::sin(yaw), 0.0f, std::cos(yaw)};
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
            const std::byte body = (static_cast<std::byte>(view.body) & k_bodyBits) << k_bodyShift;
            bytes.push_back((view.isDead ? k_deadBit : std::byte{0}) | (view.isAlert ? k_alertBit : std::byte{0})
                | (view.hitHeading ? k_hitBit : std::byte{0}) | gearLook | body);
            bytes.push_back(static_cast<std::byte>(view.shotCount));
            bytes.push_back(static_cast<std::byte>(view.hitHeading.value_or(0)));
            bytes.push_back(static_cast<std::byte>(view.hitLimb));
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
                .gearLook = gearLookOf((flags >> k_gearLookShift) & k_gearLookBits),
                .body = bodyOf((flags >> k_bodyShift) & k_bodyBits),
                .hitHeading = (flags & k_hitBit) != std::byte{0}
                    ? std::optional<uint8_t>(static_cast<uint8_t>(bytes[offset + k_entityBytes + 2]))
                    : std::nullopt,
                .hitLimb = limbOf(bytes[offset + k_entityBytes + 3])});
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
