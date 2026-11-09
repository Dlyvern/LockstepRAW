#include "Level/MorningRoutine.hpp"

#include <algorithm>
#include <iostream>

namespace game
{
    bool isNearCar(const ApartmentRoom& room, const math::Vec3& foot)
    {
        if (!room.carSpot)
        {
            return false;
        }

        const math::Vec3 offset = foot - *room.carSpot;

        return math::length(math::Vec3{offset.x, 0.0f, offset.z}) <= k_carReach;
    }

    void updateMorningAsHost(ApartmentRoom& room, const std::array<std::optional<math::Vec3>, k_playerCount>& feet)
    {
        if (room.state.phase != MorningPhase::GoToCar)
        {
            return;
        }

        for (const std::optional<math::Vec3>& foot : feet)
        {
            if (!foot || !isNearCar(room, *foot))
            {
                return;
            }
        }

        room.state.phase = MorningPhase::MorningComplete;
        recordEchoEvent(room, 0, RoomObject::Other, RoomAction::StartDrive);
        std::clog << "Game: room: both players are at the car; morning complete\n";
    }

    const char* objectiveText(const RoomState& state, std::size_t player, bool isAtCar)
    {
        switch (state.phase)
        {
            case MorningPhase::Apartment:
                if (!state.hasBadge[player] || !state.hasSidearm[player])
                {
                    return "Get your gear";
                }

                return "Head out with your flatmate";
            case MorningPhase::GoToCar:
                return isAtCar ? "Wait for your flatmate at the car" : "Head to the car";
            case MorningPhase::MorningComplete:
                break;
        }

        return "";
    }

    void stepDrive(ApartmentRoom& room, float playerDelta)
    {
        if (room.state.phase != MorningPhase::MorningComplete)
        {
            return;
        }

        if (room.driveSeconds == 0.0f)
        {
            std::clog << "Game: drive to the station\n";
        }

        room.driveSeconds += playerDelta;
    }

    float driveFade(const ApartmentRoom& room)
    {
        return std::clamp(room.driveSeconds / k_driveFadeSeconds, 0.0f, 1.0f);
    }
} //namespace game
