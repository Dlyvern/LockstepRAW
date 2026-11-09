#ifndef LOCKSTEP_GAME_LEVEL_MORNING_ROUTINE_HPP
#define LOCKSTEP_GAME_LEVEL_MORNING_ROUTINE_HPP

#include "Level/ApartmentRoom.hpp"

#include <Lockstep/Core/Math.hpp>

#include <array>
#include <cstddef>
#include <optional>
#include <string_view>

namespace game
{
    // How close to the car spot (horizontally, in metres) both players must be to start the drive.
    inline constexpr float k_carReach{3.0f};

    // Player seconds the screen takes to fade to black once the drive starts.
    inline constexpr float k_driveFadeSeconds{2.0f};

    // room:    the room.
    // foot:    a pawn's foot position.
    // Returns: whether the pawn stands within k_carReach of the car spot (false without one).
    [[nodiscard]] bool isNearCar(const ApartmentRoom& room, const math::Vec3& foot);

    // Host: moves the morning on: in GoToCar, once both players stand by the car, the drive
    // starts (MorningComplete; recorded for the Echo).
    // room: the room.
    // feet: per player, their pawn's foot position, or nothing while absent.
    void updateMorningAsHost(ApartmentRoom& room, const std::array<std::optional<math::Vec3>, k_playerCount>& feet);

    // The small objective line for one player ("Get your gear", "Head to the car").
    // state:     the room state.
    // player:    the player.
    // isAtCar:   whether that player already stands by the car.
    // Returns:   the line's dialogue id ("morning.objective.*", see lineText()), empty when there
    //            is nothing to do.
    [[nodiscard]] std::string_view objectiveId(const RoomState& state, std::size_t player, bool isAtCar);

    // Both machines, every tick: counts the drive's fade and logs when the drive starts.
    // room:        the room.
    // playerDelta: Player domain delta of this tick.
    void stepDrive(ApartmentRoom& room, float playerDelta);

    // room:    the room.
    // Returns: how far the screen has faded to black, 0 (not at all) to 1 (black).
    [[nodiscard]] float driveFade(const ApartmentRoom& room);
} //namespace game

#endif //LOCKSTEP_GAME_LEVEL_MORNING_ROUTINE_HPP
