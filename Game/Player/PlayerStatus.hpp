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
    // Bytes an encoded StatusMessage takes.
    inline constexpr std::size_t k_playerStatusBytes{2 + 2 * k_playerCount};

    // What a player sees of a pawn's health and weapon. The host sends both players' statuses to
    // the client in a reliable Game message whenever one changes.
    // health:   hit points left.
    // isJoined: the player is in the game.
    // isDead:   the pawn is dead and waits to respawn.
    // isArmed:  the player carries the weapon.
    // isAiming: the armed player aims down sights (drawn on the partner's machine).
    // isLowered: the armed player's weapon is lowered.
    // isCrouched: the pawn is crouched or sliding (its capsule is low).
    // isSliding: the pawn slides.
    struct PlayerStatus
    {
        int32_t health{0};
        bool isJoined{false};
        bool isDead{false};
        bool isArmed{false};
        bool isAiming{false};
        bool isLowered{false};
        bool isCrouched{false};
        bool isSliding{false};

        friend bool operator==(const PlayerStatus&, const PlayerStatus&) = default;
    };

    // Both players' statuses, indexed like GameState::Simulation::pawns.
    using PlayerStatuses = std::array<PlayerStatus, k_playerCount>;

    // The host's reliable status message: both players' statuses plus game-wide flags.
    // players:        both players' statuses.
    // isCombatActive: a combat situation is running; safe zones then stop lowering weapons.
    struct StatusMessage
    {
        PlayerStatuses players{};
        bool isCombatActive{false};

        friend bool operator==(const StatusMessage&, const StatusMessage&) = default;
    };

    // pawn:    a pawn on the host.
    // Returns: its status.
    [[nodiscard]] PlayerStatus playerStatus(const Pawn& pawn);

    // Packs the status message into a Game message. Health is clamped to [0, 255].
    // message: both players' statuses and the game-wide flags.
    // Returns: the message bytes.
    [[nodiscard]] std::array<std::byte, k_playerStatusBytes> encodeStatusMessage(const StatusMessage& message);

    // Unpacks a Game message.
    // bytes:   a received Game message.
    // Returns: the status message, or nothing if `bytes` is not one.
    [[nodiscard]] std::optional<StatusMessage> decodeStatusMessage(std::span<const std::byte> bytes);
} //namespace game

#endif //LOCKSTEP_GAME_PLAYER_PLAYER_STATUS_HPP
