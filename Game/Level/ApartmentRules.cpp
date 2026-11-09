#include "Level/ApartmentRoom.hpp"

#include <algorithm>
#include <iostream>

namespace game
{
    namespace
    {
        constexpr float k_hostReachTolerance{0.3f};
        constexpr float k_brewSeconds{6.0f};
        constexpr float k_doorWaitSeconds{4.0f};
        constexpr float k_lockerOpenSeconds{0.6f};
        constexpr float k_doorOpenSeconds{1.0f};
        constexpr float k_animationStart{1.0f / 255.0f};

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

        RoomAction useLocker(RoomState& state, std::size_t player, std::size_t owner)
        {
            if (owner != player)
            {
                return refuse(state, player, RoomNotice::NotYourLocker);
            }

            if (state.lockerOpen[owner] == 0.0f)
            {
                state.lockerOpen[owner] = k_animationStart;

                return RoomAction::OpenLocker;
            }

            if (hasGear(state, owner))
            {
                return refuse(state, player, RoomNotice::LockerEmpty);
            }

            state.hasBadge[owner] = true;
            state.hasSidearm[owner] = true;

            return RoomAction::TakeGear;
        }

        RoomAction useFrontDoor(ApartmentRoom& room, std::size_t player, bool isPartnerJoined)
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

            if (!isPartnerJoined)
            {
                return refuse(state, player, RoomNotice::PartnerAbsent);
            }

            if (!hasGear(state, partner))
            {
                return refuse(state, player, RoomNotice::PartnerNeedsGear);
            }

            state.isWaitingAtDoor[player] = true;
            room.doorWaitSeconds[player] = k_doorWaitSeconds;

            if (!state.isWaitingAtDoor[partner])
            {
                return tell(state, player, RoomNotice::WaitingForPartner, RoomAction::ReadyAtDoor);
            }

            state.isWaitingAtDoor = {};
            room.doorWaitSeconds = {};
            state.door = DoorPhase::Opening;
            state.doorOpen = k_animationStart;
            std::clog << "Game: room: both players are at the front door, it opens\n";

            return RoomAction::ReadyAtDoor;
        }

        RoomAction applyRules(ApartmentRoom& room, RoomObject object, std::size_t player, bool isPartnerJoined)
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
                    return useLocker(state, player, 0);
                case RoomObject::Locker2:
                    return useLocker(state, player, 1);
                case RoomObject::FrontDoor:
                    return useFrontDoor(room, player, isPartnerJoined);
                case RoomObject::WallClock:
                    return tell(state, player, RoomNotice::WallClockTime, RoomAction::Inspect);
                case RoomObject::Other:
                    break;
            }

            return tell(state, player, RoomNotice::NothingHappens, RoomAction::Inspect);
        }

        float advance(float value, float delta, float seconds)
        {
            return value > 0.0f && value < 1.0f ? std::min(value + delta / seconds, 1.0f) : value;
        }
    } //namespace

    void interactAsHost(ApartmentRoom& room, const SceneLevel& level, const lockstep::physics::PhysicsWorld& physics,
        std::size_t player, const math::Vec3& eye, const math::Vec3& direction,
        lockstep::physics::ControllerId ignore, bool isPartnerJoined)
    {
        const std::optional<LookTarget> target = traceInteractable(room.interactables, level, physics, eye, direction,
            k_interactReach + k_hostReachTolerance, ignore);

        if (!target)
        {
            std::clog << "Game: room: " << playerName(player) << " pressed E at nothing\n";
            return;
        }

        const RoomObject object = room.objects[target->interactable];
        const RoomAction action = applyRules(room, object, player, isPartnerJoined);
        std::clog << "Game: room: " << playerName(player) << " used " << room.interactables[target->interactable].name
            << ": " << toString(action) << '\n';

        if (room.isRecording)
        {
            room.recording.events.push_back(EchoEvent{.frame = static_cast<uint32_t>(room.recording.frames.size()),
                .player = static_cast<uint8_t>(player), .object = object, .action = action});
        }
    }

    void updateRoomAsHost(ApartmentRoom& room, float worldDelta, float playerDelta)
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
            state.lockerOpen[player] = advance(state.lockerOpen[player], worldDelta, k_lockerOpenSeconds);

            if (state.isWaitingAtDoor[player])
            {
                room.doorWaitSeconds[player] -= playerDelta;
                state.isWaitingAtDoor[player] = room.doorWaitSeconds[player] > 0.0f;
            }
        }

        if (state.door == DoorPhase::Opening)
        {
            state.doorOpen = advance(state.doorOpen, worldDelta, k_doorOpenSeconds);

            if (state.doorOpen >= 1.0f)
            {
                state.door = DoorPhase::Open;
                std::clog << "Game: room complete\n";
            }
        }
    }
} //namespace game
