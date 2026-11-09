#include "Level/ApartmentRoom.hpp"

#include <algorithm>
#include <iostream>

namespace game
{
    namespace
    {
        constexpr float k_hostReachTolerance{0.3f};
        constexpr float k_brewSeconds{6.0f};
        constexpr float k_lockerOpenSeconds{0.6f};
        constexpr float k_doorOpenSeconds{1.0f};
        constexpr float k_streetDoorSeconds{1.2f};
        constexpr float k_animationStart{1.0f / 255.0f};
        constexpr float k_partnerDoorReach{3.0f};

        std::size_t partnerOf(std::size_t player)
        {
            return player == 0 ? 1 : 0;
        }

        bool hasGear(const RoomState& state, std::size_t player)
        {
            return state.hasBadge[player] && state.hasSidearm[player];
        }

        bool hasMug(const RoomState& state, std::size_t player)
        {
            return std::find(state.mugTakenBy.begin(), state.mugTakenBy.end(), player) != state.mugTakenBy.end();
        }

        RoomAction refuse(RoomState& state, std::size_t player, RoomNotice notice)
        {
            ++state.noticeCount[player];
            state.notice[player] = notice;

            return RoomAction::Refused;
        }

        RoomAction tell(RoomState& state, std::size_t player, RoomNotice notice, RoomAction action)
        {
            ++state.noticeCount[player];
            state.notice[player] = notice;

            return action;
        }

        RoomAction takeMug(RoomState& state, std::size_t player, std::optional<std::size_t> wanted)
        {
            if (state.coffee == CoffeePhase::Brewing)
            {
                return refuse(state, player, RoomNotice::StillBrewing);
            }

            if (state.coffee != CoffeePhase::Ready)
            {
                return refuse(state, player, RoomNotice::CoffeeNotReady);
            }

            if (hasMug(state, player))
            {
                return refuse(state, player, RoomNotice::AlreadyHaveCoffee);
            }

            const std::size_t preferred = wanted.value_or(player);
            const std::size_t other = preferred == 0 ? 1 : 0;
            const std::size_t mug = state.mugTakenBy[preferred] == k_nobody ? preferred : other;

            if (state.mugTakenBy[mug] != k_nobody || (wanted && mug != *wanted))
            {
                return refuse(state, player, RoomNotice::NothingHappens);
            }

            state.mugTakenBy[mug] = static_cast<uint8_t>(player);

            return RoomAction::TakeMug;
        }

        RoomAction useCoffeeMachine(ApartmentRoom& room, std::size_t player)
        {
            if (room.state.coffee == CoffeePhase::Idle)
            {
                room.state.coffee = CoffeePhase::Brewing;
                room.brewSeconds = k_brewSeconds;

                return RoomAction::StartBrew;
            }

            return takeMug(room.state, player, std::nullopt);
        }

        RoomAction takeGear(RoomState& state, std::size_t player, std::size_t owner)
        {
            if (owner != player)
            {
                return refuse(state, player, RoomNotice::NotYourLocker);
            }

            if (hasGear(state, owner))
            {
                return RoomAction::Inspect;
            }

            state.hasBadge[owner] = true;
            state.hasSidearm[owner] = true;

            return RoomAction::TakeGear;
        }

        RoomAction toggleLocker(ApartmentRoom& room, std::size_t player, std::size_t owner)
        {
            RoomState& state = room.state;
            state.isLockerOpen[owner] = !state.isLockerOpen[owner];

            if (!state.isLockerOpen[owner])
            {
                return RoomAction::CloseLocker;
            }

            if (room.gear[owner].empty() && owner == player && !hasGear(state, owner))
            {
                std::clog << "Game: room: no Interact_Gear_" << owner + 1 << " node; the gear is taken with the door\n";
                return takeGear(state, player, owner);
            }

            return RoomAction::OpenLocker;
        }

        bool isNearFrontDoor(const ApartmentRoom& room, const std::optional<math::Vec3>& foot)
        {
            if (!foot || !room.frontDoorSpot)
            {
                return false;
            }

            const math::Vec3 offset = *foot - *room.frontDoorSpot;

            return math::length(math::Vec3{offset.x, 0.0f, offset.z}) <= k_partnerDoorReach;
        }

        RoomAction useFrontDoor(ApartmentRoom& room, std::size_t player, const std::optional<math::Vec3>& partnerFoot)
        {
            RoomState& state = room.state;
            const std::size_t partner = partnerOf(player);

            if (state.door != DoorPhase::Closed)
            {
                return RoomAction::Inspect;
            }

            if (!hasGear(state, player))
            {
                return refuse(state, player, RoomNotice::NeedGear);
            }

            if (!partnerFoot)
            {
                return refuse(state, player, RoomNotice::PartnerAbsent);
            }

            if (!hasGear(state, partner))
            {
                return refuse(state, player, RoomNotice::PartnerNeedsGear);
            }

            if (!isNearFrontDoor(room, partnerFoot))
            {
                return refuse(state, player, RoomNotice::PartnerAbsent);
            }

            state.door = DoorPhase::Opening;
            state.doorOpen = k_animationStart;
            state.phase = MorningPhase::GoToCar;
            std::clog << "Game: room: " << playerName(player)
                << " opens the front door with their flatmate close by; morning phase: go to the car\n";

            return RoomAction::OpenFrontDoor;
        }

        RoomAction useStreetDoor(RoomState& state, std::size_t player)
        {
            state.isStreetDoorOpen = !state.isStreetDoorOpen;
            state.streetDoorBy = static_cast<uint8_t>(player);

            return state.isStreetDoorOpen ? RoomAction::OpenStreetDoor : RoomAction::CloseStreetDoor;
        }

        RoomAction applyRules(ApartmentRoom& room, RoomObject object, std::size_t player,
            const std::optional<math::Vec3>& partnerFoot)
        {
            RoomState& state = room.state;

            switch (object)
            {
                case RoomObject::AlarmClock:
                    if (!state.isAlarmRinging)
                    {
                        return refuse(state, player, RoomNotice::AlarmAlreadyOff);
                    }

                    state.isAlarmRinging = false;
                    state.alarmSilencedBy = static_cast<uint8_t>(player);

                    return RoomAction::SilenceAlarm;
                case RoomObject::CoffeeMachine:
                    return useCoffeeMachine(room, player);
                case RoomObject::MugA:
                    return takeMug(state, player, 0);
                case RoomObject::MugB:
                    return takeMug(state, player, 1);
                case RoomObject::Radio:
                    state.isRadioOn = !state.isRadioOn;

                    return RoomAction::ToggleRadio;
                case RoomObject::Mirror:
                    state.hasUsedMirror[player] = true;

                    return tell(state, player, RoomNotice::MirrorLook, RoomAction::UseMirror);
                case RoomObject::Locker1:
                    return toggleLocker(room, player, 0);
                case RoomObject::Locker2:
                    return toggleLocker(room, player, 1);
                case RoomObject::Gear1:
                    return takeGear(state, player, 0);
                case RoomObject::Gear2:
                    return takeGear(state, player, 1);
                case RoomObject::FrontDoor:
                    return useFrontDoor(room, player, partnerFoot);
                case RoomObject::WallClock:
                    return tell(state, player, RoomNotice::WallClockTime, RoomAction::Inspect);
                case RoomObject::StreetDoor:
                    return useStreetDoor(state, player);
                case RoomObject::Toy:
                case RoomObject::Other:
                    break;
            }

            return tell(state, player, RoomNotice::NothingHappens, RoomAction::Inspect);
        }

        float advance(float value, float delta, float seconds)
        {
            return value > 0.0f && value < 1.0f ? std::min(value + delta / seconds, 1.0f) : value;
        }

        float moveTowards(float value, bool isOpen, float delta, float seconds)
        {
            return isOpen ? std::min(value + delta / seconds, 1.0f) : std::max(value - delta / seconds, 0.0f);
        }
    } //namespace

    std::optional<RoomAction> interactAsHost(ApartmentRoom& room, const SceneLevel& level,
        const lockstep::physics::PhysicsWorld& physics, std::size_t player, const math::Vec3& eye,
        const math::Vec3& direction, lockstep::physics::ControllerId ignore, const std::optional<math::Vec3>& partnerFoot)
    {
        const std::optional<LookTarget> target = traceInteractable(room.interactables, level, physics, eye, direction,
            k_interactReach + k_hostReachTolerance, ignore);

        if (!target)
        {
            std::clog << "Game: room: " << playerName(player) << " pressed E at nothing\n";
            return std::nullopt;
        }

        const RoomObject object = room.objects[target->interactable];
        const RoomAction action = applyRules(room, object, player, partnerFoot);
        std::clog << "Game: room: " << playerName(player) << " used " << room.interactables[target->interactable].name
            << ": " << toString(action) << '\n';

        recordEchoEvent(room, player, object, action);

        return action;
    }

    void recordEchoEvent(ApartmentRoom& room, std::size_t player, RoomObject object, RoomAction action)
    {
        if (room.isRecording)
        {
            room.recording.events.push_back(EchoEvent{.frame = static_cast<uint32_t>(room.recording.frames.size()),
                .player = static_cast<uint8_t>(player), .object = object, .action = action});
        }
    }

    void updateRoomAsHost(ApartmentRoom& room, float worldDelta)
    {
        RoomState& state = room.state;

        if (state.coffee == CoffeePhase::Brewing)
        {
            room.brewSeconds -= worldDelta;

            if (room.brewSeconds <= 0.0f)
            {
                state.coffee = CoffeePhase::Ready;
            }
        }

        for (std::size_t player = 0; player < k_playerCount; ++player)
        {
            state.lockerOpen[player] = moveTowards(state.lockerOpen[player], state.isLockerOpen[player], worldDelta,
                k_lockerOpenSeconds);
        }

        if (state.door == DoorPhase::Opening)
        {
            state.doorOpen = advance(state.doorOpen, worldDelta, k_doorOpenSeconds);

            if (state.doorOpen >= 1.0f)
            {
                state.door = DoorPhase::Open;
            }
        }

        state.streetDoorOpen = moveTowards(state.streetDoorOpen, state.isStreetDoorOpen, worldDelta,
            k_streetDoorSeconds);
    }
} //namespace game
