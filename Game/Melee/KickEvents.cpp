#include "Melee/KickEvents.hpp"

#include "Player/WireBytes.hpp"

#include <Lockstep/Net/Session.hpp>

#include <cmath>
#include <iostream>

namespace game
{
    namespace
    {
        namespace net = lockstep::net;

        constexpr std::byte k_kickTag{0x4B};
        constexpr std::size_t k_pawnOffset{1};
        constexpr std::size_t k_flagsOffset{2};
        constexpr std::size_t k_originOffset{3};
        constexpr std::size_t k_directionOffset{15};
        constexpr std::size_t k_kickBytes{27};
        constexpr std::byte k_hitBit{0x01};

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

        bool isFinite(const math::Vec3& value)
        {
            return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
        }
    } //namespace

    std::vector<std::byte> encodeKickEvent(const KickEvent& event)
    {
        std::vector<std::byte> bytes(k_kickBytes);
        bytes[0] = k_kickTag;
        bytes[k_pawnOffset] = static_cast<std::byte>(event.pawn);
        bytes[k_flagsOffset] = event.isHit ? k_hitBit : std::byte{0};
        writeVec3(bytes, k_originOffset, event.origin);
        writeVec3(bytes, k_directionOffset, event.direction);

        return bytes;
    }

    std::optional<KickEvent> decodeKickEvent(std::span<const std::byte> bytes)
    {
        if (bytes.size() != k_kickBytes || bytes[0] != k_kickTag)
        {
            return std::nullopt;
        }

        const KickEvent event{.pawn = static_cast<uint8_t>(bytes[k_pawnOffset]),
            .origin = readVec3(bytes, k_originOffset),
            .direction = readVec3(bytes, k_directionOffset),
            .isHit = (bytes[k_flagsOffset] & k_hitBit) != std::byte{0}};

        if (!isFinite(event.origin) || !isFinite(event.direction))
        {
            return std::nullopt;
        }

        return event;
    }

    void publishKickEvents(std::vector<KickEvent>& events, std::vector<KickEvent>& shown, net::Session& session)
    {
        const bool isConnected = session.state() == net::ConnectionState::Connected;

        for (const KickEvent& event : events)
        {
            if (isConnected)
            {
                if (auto sent = session.send(encodeKickEvent(event), net::Delivery::Reliable); !sent)
                {
                    std::clog << "Game: kick not sent: " << sent.error().message << '\n';
                }
            }

            shown.push_back(event);
        }

        events.clear();
    }
} //namespace game
