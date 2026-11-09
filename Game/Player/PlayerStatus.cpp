#include "Player/PlayerStatus.hpp"

#include <algorithm>

namespace game
{
    namespace
    {
        constexpr std::byte k_statusTag{0x48};
        constexpr std::byte k_joinedBit{0x01};
        constexpr std::byte k_deadBit{0x02};
        constexpr std::byte k_armedBit{0x04};
        constexpr std::byte k_aimingBit{0x08};
        constexpr std::byte k_loweredBit{0x10};
        constexpr std::byte k_crouchedBit{0x20};
        constexpr std::byte k_slidingBit{0x40};
        constexpr std::byte k_combatBit{0x01};
        constexpr std::size_t k_gameFlagsOffset{k_playerStatusBytes - 1};
        constexpr int32_t k_maxEncodedHealth{255};
        constexpr std::size_t k_tagBytes{1};
        constexpr std::size_t k_bytesPerPlayer{2};
    } //namespace

    PlayerStatus playerStatus(const Pawn& pawn)
    {
        return PlayerStatus{.health = pawn.health,
            .isJoined = pawn.isJoined,
            .isDead = pawn.isJoined && pawn.entity.isNull(),
            .isArmed = pawn.isArmed,
            .isAiming = pawn.isArmed && pawn.hasCommand && pawn.command.aim,
            .isLowered = pawn.isArmed && pawn.isLowered,
            .isCrouched = !pawn.entity.isNull() && pawn.motion.stance != PawnStance::Stand,
            .isSliding = !pawn.entity.isNull() && isSliding(pawn.motion)};
    }

    std::array<std::byte, k_playerStatusBytes> encodeStatusMessage(const StatusMessage& message)
    {
        const PlayerStatuses& statuses = message.players;
        std::array<std::byte, k_playerStatusBytes> bytes{};
        bytes[0] = k_statusTag;
        bytes[k_gameFlagsOffset] = message.isCombatActive ? k_combatBit : std::byte{0};

        for (std::size_t index = 0; index < statuses.size(); ++index)
        {
            const PlayerStatus& status = statuses[index];
            const std::size_t offset = k_tagBytes + index * k_bytesPerPlayer;
            bytes[offset] = static_cast<std::byte>(std::clamp(status.health, 0, k_maxEncodedHealth));
            bytes[offset + 1] = (status.isJoined ? k_joinedBit : std::byte{0}) | (status.isDead ? k_deadBit : std::byte{0})
                | (status.isArmed ? k_armedBit : std::byte{0}) | (status.isAiming ? k_aimingBit : std::byte{0})
                | (status.isLowered ? k_loweredBit : std::byte{0}) | (status.isCrouched ? k_crouchedBit : std::byte{0})
                | (status.isSliding ? k_slidingBit : std::byte{0});
        }

        return bytes;
    }

    std::optional<StatusMessage> decodeStatusMessage(std::span<const std::byte> bytes)
    {
        if (bytes.size() != k_playerStatusBytes || bytes[0] != k_statusTag)
        {
            return std::nullopt;
        }

        StatusMessage message{.isCombatActive = (bytes[k_gameFlagsOffset] & k_combatBit) != std::byte{0}};
        PlayerStatuses& statuses = message.players;

        for (std::size_t index = 0; index < statuses.size(); ++index)
        {
            const std::size_t offset = k_tagBytes + index * k_bytesPerPlayer;
            const std::byte flags = bytes[offset + 1];
            statuses[index] = PlayerStatus{.health = static_cast<int32_t>(bytes[offset]),
                .isJoined = (flags & k_joinedBit) != std::byte{0},
                .isDead = (flags & k_deadBit) != std::byte{0},
                .isArmed = (flags & k_armedBit) != std::byte{0},
                .isAiming = (flags & k_aimingBit) != std::byte{0},
                .isLowered = (flags & k_loweredBit) != std::byte{0},
                .isCrouched = (flags & k_crouchedBit) != std::byte{0},
                .isSliding = (flags & k_slidingBit) != std::byte{0}};
        }

        return message;
    }
} //namespace game
