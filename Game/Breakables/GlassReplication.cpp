#include "Breakables/GlassReplication.hpp"

#include "Player/WireBytes.hpp"

#include <Lockstep/Net/Session.hpp>

#include <algorithm>
#include <iostream>

namespace game
{
    namespace
    {
        namespace net = lockstep::net;

        constexpr std::byte k_glassTag{0x42};
        constexpr std::size_t k_headerBytes{5};
        constexpr std::size_t k_breakBytes{2 + 12 + 12};

        void writeUint16(std::span<std::byte> bytes, std::size_t offset, std::size_t value)
        {
            bytes[offset] = static_cast<std::byte>(value & 0xFFu);
            bytes[offset + 1] = static_cast<std::byte>((value >> 8) & 0xFFu);
        }

        uint16_t readUint16(std::span<const std::byte> bytes, std::size_t offset)
        {
            return static_cast<uint16_t>(static_cast<uint32_t>(bytes[offset])
                | (static_cast<uint32_t>(bytes[offset + 1]) << 8));
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

        std::size_t bitsetBytes(std::size_t panes)
        {
            return (panes + 7) / 8;
        }
    } //namespace

    std::vector<std::byte> encodeGlassMessage(const GlassMessage& message)
    {
        const std::size_t panes = std::min(message.isBroken.size(), k_maxGlassPanes);
        const std::size_t breaks = std::min(message.breaks.size(), k_maxGlassBreaksPerMessage);
        const std::size_t bitsetOffset = k_headerBytes;
        const std::size_t breaksOffset = bitsetOffset + bitsetBytes(panes);
        std::vector<std::byte> bytes(breaksOffset + breaks * k_breakBytes);
        bytes[0] = k_glassTag;
        writeUint16(bytes, 1, panes);
        writeUint16(bytes, 3, breaks);

        for (std::size_t pane = 0; pane < panes; ++pane)
        {
            if (message.isBroken[pane])
            {
                bytes[bitsetOffset + pane / 8] |= static_cast<std::byte>(1u << (pane % 8));
            }
        }

        for (std::size_t index = 0; index < breaks; ++index)
        {
            const GlassBreak& hit = message.breaks[index];
            const std::size_t offset = breaksOffset + index * k_breakBytes;
            writeUint16(bytes, offset, hit.pane);
            writeVec3(bytes, offset + 2, hit.point);
            writeVec3(bytes, offset + 14, hit.direction);
        }

        return bytes;
    }

    std::optional<GlassMessage> decodeGlassMessage(std::span<const std::byte> bytes)
    {
        if (bytes.size() < k_headerBytes || bytes[0] != k_glassTag)
        {
            return std::nullopt;
        }

        const std::size_t panes = readUint16(bytes, 1);
        const std::size_t breaks = readUint16(bytes, 3);
        const std::size_t breaksOffset = k_headerBytes + bitsetBytes(panes);

        if (panes > k_maxGlassPanes || breaks > k_maxGlassBreaksPerMessage
            || bytes.size() != breaksOffset + breaks * k_breakBytes)
        {
            return std::nullopt;
        }

        GlassMessage message{};
        message.isBroken.resize(panes);

        for (std::size_t pane = 0; pane < panes; ++pane)
        {
            message.isBroken[pane] = (bytes[k_headerBytes + pane / 8] & static_cast<std::byte>(1u << (pane % 8)))
                != std::byte{0};
        }

        for (std::size_t index = 0; index < breaks; ++index)
        {
            const std::size_t offset = breaksOffset + index * k_breakBytes;
            const GlassBreak hit{.pane = readUint16(bytes, offset),
                .point = readVec3(bytes, offset + 2),
                .direction = readVec3(bytes, offset + 14)};

            if (hit.pane >= panes)
            {
                return std::nullopt;
            }

            message.breaks.push_back(hit);
        }

        return message;
    }

    void publishGlass(BreakableGlass& glass, net::Session& session)
    {
        if (session.state() != net::ConnectionState::Connected)
        {
            glass.isSent = false;
            glass.unsent.clear();
            return;
        }

        if (glass.panes.empty() || (glass.isSent && glass.unsent.empty()))
        {
            return;
        }

        GlassMessage message{.isBroken = glass.isBroken, .breaks = {}};
        const std::size_t breaks = std::min(glass.unsent.size(), k_maxGlassBreaksPerMessage);
        message.breaks.assign(glass.unsent.end() - static_cast<std::ptrdiff_t>(breaks), glass.unsent.end());

        if (auto sent = session.send(encodeGlassMessage(message), net::Delivery::Reliable); !sent)
        {
            std::clog << "Game: glass state not sent: " << sent.error().message << '\n';
            return;
        }

        glass.isSent = true;
        glass.unsent.clear();
    }

    bool receiveGlassMessage(BreakableGlass& glass, SceneLevel* level, lockstep::physics::PhysicsWorld& physics,
        std::span<const std::byte> bytes)
    {
        const std::optional<GlassMessage> message = decodeGlassMessage(bytes);

        if (!message)
        {
            return false;
        }

        if (level == nullptr || message->isBroken.size() != glass.panes.size())
        {
            return true;
        }

        for (const GlassBreak& hit : message->breaks)
        {
            if (applyGlassBreak(glass, *level, physics, hit))
            {
                std::clog << "Game: glass pane '" << glass.panes[hit.pane].name << "' shattered (host)\n";
            }
        }

        const std::size_t shownBefore = glass.shown.size();

        for (std::size_t pane = 0; pane < glass.panes.size(); ++pane)
        {
            if (message->isBroken[pane])
            {
                applyGlassBreak(glass, *level, physics, GlassBreak{.pane = static_cast<uint16_t>(pane)});
            }
        }

        const std::size_t silent = glass.shown.size() - shownBefore;
        glass.shown.resize(shownBefore);

        if (silent > 0)
        {
            std::clog << "Game: " << silent << " glass panes were already broken\n";
        }

        return true;
    }
} //namespace game
