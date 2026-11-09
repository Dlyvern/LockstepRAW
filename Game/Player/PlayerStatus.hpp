#ifndef LOCKSTEP_GAME_PLAYER_PLAYER_STATUS_HPP
#define LOCKSTEP_GAME_PLAYER_PLAYER_STATUS_HPP

#include "Player/Pawns.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

namespace game
{
    // Bytes an encoded PlayerStatuses message takes.
    inline constexpr std::size_t k_playerStatusBytes{1 + 2 * k_playerCount};

    // What a player sees of a pawn's health and weapon. The host sends both players' statuses to
    // the client in a reliable Game message whenever one changes.
    // health:   hit points left.
    // isJoined: the player is in the game.
    // isDead:   the pawn is dead and waits to respawn.
    // isArmed:  the player carries the weapon.
    // isAiming: the armed player aims down sights (drawn on the partner's machine).
    // isLowered: the armed player's weapon is lowered.
    struct PlayerStatus
    {
        int32_t health{0};
        bool isJoined{false};
        bool isDead{false};
        bool isArmed{false};
        bool isAiming{false};
        bool isLowered{false};

        friend bool operator==(const PlayerStatus&, const PlayerStatus&) = default;
    };

    // Both players' statuses, indexed like GameState::Simulation::pawns.
    using PlayerStatuses = std::array<PlayerStatus, k_playerCount>;

    // pawn:    a pawn on the host.
    // Returns: its status.
    [[nodiscard]] PlayerStatus playerStatus(const Pawn& pawn);

    // Packs the statuses into a Game message. Health is clamped to [0, 255].
    // statuses: both players' statuses.
    // Returns:  the message bytes.
    [[nodiscard]] std::array<std::byte, k_playerStatusBytes> encodePlayerStatuses(const PlayerStatuses& statuses);

    // Unpacks a Game message.
    // bytes:   a received Game message.
    // Returns: the statuses, or nothing if `bytes` is not a status message.
    [[nodiscard]] std::optional<PlayerStatuses> decodePlayerStatuses(std::span<const std::byte> bytes);
} //namespace game

#endif //LOCKSTEP_GAME_PLAYER_PLAYER_STATUS_HPP
