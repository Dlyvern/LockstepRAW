#include "Player/PlayerStatus.hpp"

#include <algorithm>

namespace game
{
    namespace
    {
        constexpr std::byte k_statusTag{0x48};
        constexpr std::byte k_joinedBit{0x01};
        constexpr std::byte k_deadBit{0x02};
        constexpr int32_t k_maxEncodedHealth{255};
        constexpr std::size_t k_tagBytes{1};
        constexpr std::size_t k_bytesPerPlayer{2};
    } //namespace

    PlayerStatus playerStatus(const Pawn& pawn)
    {
        return PlayerStatus{.health = pawn.health,
            .isJoined = pawn.isJoined,
            .isDead = pawn.isJoined && pawn.entity.isNull()};
    }

    std::array<std::byte, k_playerStatusBytes> encodePlayerStatuses(const PlayerStatuses& statuses)
    {
        std::array<std::byte, k_playerStatusBytes> bytes{};
        bytes[0] = k_statusTag;

        for (std::size_t index = 0; index < statuses.size(); ++index)
        {
            const PlayerStatus& status = statuses[index];
            const std::size_t offset = k_tagBytes + index * k_bytesPerPlayer;
            bytes[offset] = static_cast<std::byte>(std::clamp(status.health, 0, k_maxEncodedHealth));
            bytes[offset + 1] = (status.isJoined ? k_joinedBit : std::byte{0}) | (status.isDead ? k_deadBit : std::byte{0});
        }

        return bytes;
    }

    std::optional<PlayerStatuses> decodePlayerStatuses(std::span<const std::byte> bytes)
    {
        if (bytes.size() != k_playerStatusBytes || bytes[0] != k_statusTag)
        {
            return std::nullopt;
        }

        PlayerStatuses statuses{};

        for (std::size_t index = 0; index < statuses.size(); ++index)
        {
            const std::size_t offset = k_tagBytes + index * k_bytesPerPlayer;
            const std::byte flags = bytes[offset + 1];
            statuses[index] = PlayerStatus{.health = static_cast<int32_t>(bytes[offset]),
                .isJoined = (flags & k_joinedBit) != std::byte{0},
                .isDead = (flags & k_deadBit) != std::byte{0}};
        }

        return statuses;
    }
} //namespace game
