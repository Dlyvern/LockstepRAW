#include "Level/RoomState.hpp"

#include <cmath>
#include <utility>

namespace game
{
    namespace
    {
        constexpr std::byte k_roomTag{0x52};
        constexpr std::byte k_alarmRingingBit{0x01};
        constexpr std::byte k_radioOnBit{0x02};
        constexpr std::byte k_streetDoorOpenBit{0x04};
        constexpr float k_quantization{255.0f};
        constexpr uint8_t k_badgeShift{0};
        constexpr uint8_t k_sidearmShift{2};
        constexpr uint8_t k_mirrorShift{4};
        constexpr uint8_t k_lockerOpenShift{6};

        constexpr std::array<std::pair<std::string_view, RoomObject>, 13> k_objectNames{{
            {"Interact_AlarmClock", RoomObject::AlarmClock},
            {"Interact_CoffeeMachine", RoomObject::CoffeeMachine},
            {"Mug_A", RoomObject::MugA},
            {"Mug_B", RoomObject::MugB},
            {"Interact_Radio", RoomObject::Radio},
            {"Interact_Mirror", RoomObject::Mirror},
            {"Interact_Locker_1", RoomObject::Locker1},
            {"Interact_Locker_2", RoomObject::Locker2},
            {"Interact_Gear_1", RoomObject::Gear1},
            {"Interact_Gear_2", RoomObject::Gear2},
            {"Interact_FrontDoor", RoomObject::FrontDoor},
            {"Interact_WallClock", RoomObject::WallClock},
            {"Interact_StreetDoor", RoomObject::StreetDoor}}};

        enum class Offset : std::size_t
        {
            Tag,
            Flags,
            AlarmSilencedBy,
            Coffee,
            MugA,
            MugB,
            Locker1,
            Locker2,
            PlayerBits,
            Door,
            DoorOpen,
            NoticeCount1,
            NoticeCount2,
            Notice1,
            Notice2,
            Phase,
            StreetDoorOpen,
            StreetDoorBy,
            HeldToy1,
            HeldToy2
        };

        constexpr std::size_t at(Offset offset)
        {
            return static_cast<std::size_t>(offset);
        }

        std::byte quantize(float value)
        {
            return static_cast<std::byte>(std::lround(math::clamp(value, 0.0f, 1.0f) * k_quantization));
        }

        float dequantize(std::byte value)
        {
            return static_cast<float>(value) / k_quantization;
        }

        uint8_t pairBits(const std::array<bool, k_playerCount>& flags, uint8_t shift)
        {
            return static_cast<uint8_t>(((flags[0] ? 1u : 0u) | (flags[1] ? 2u : 0u)) << shift);
        }

        std::array<bool, k_playerCount> readPairBits(uint8_t bits, uint8_t shift)
        {
            return {((bits >> shift) & 1u) != 0, ((bits >> shift) & 2u) != 0};
        }

        bool validPlayer(uint8_t player)
        {
            return player == k_nobody || player < k_playerCount;
        }

        bool validToy(uint8_t toy)
        {
            return toy == k_nobody || toy < k_maxToys;
        }

        void describeStreetDoor(const RoomState& before, const RoomState& after, std::size_t player,
            std::vector<std::string_view>& lines)
        {
            if (before.isStreetDoorOpen == after.isStreetDoorOpen)
            {
                return;
            }

            const bool isOwn = after.streetDoorBy == player;

            if (after.isStreetDoorOpen)
            {
                lines.push_back(isOwn ? "morning.street_door.open.self" : "morning.street_door.open.partner");
                return;
            }

            lines.push_back(isOwn ? "morning.street_door.shut.self" : "morning.street_door.shut.partner");
        }

        void describePhase(const RoomState& before, const RoomState& after, std::vector<std::string_view>& lines)
        {
            if (before.phase == after.phase)
            {
                return;
            }

            if (after.phase == MorningPhase::GoToCar)
            {
                lines.push_back("morning.narration.go_to_car");
            }
            else if (after.phase == MorningPhase::MorningComplete)
            {
                lines.push_back("morning.narration.drive");
            }
        }

        std::string_view noticeId(RoomNotice notice)
        {
            switch (notice)
            {
                case RoomNotice::AlarmAlreadyOff:
                    return "morning.notice.alarm_off";
                case RoomNotice::StillBrewing:
                    return "morning.notice.still_brewing";
                case RoomNotice::CoffeeNotReady:
                    return "morning.notice.coffee_not_ready";
                case RoomNotice::AlreadyHaveCoffee:
                    return "morning.notice.have_coffee";
                case RoomNotice::NotYourLocker:
                    return "morning.notice.not_your_locker";
                case RoomNotice::NeedGear:
                    return "morning.notice.need_gear";
                case RoomNotice::PartnerNeedsGear:
                    return "morning.notice.partner_needs_gear";
                case RoomNotice::PartnerAbsent:
                    return "morning.notice.partner_absent";
                case RoomNotice::MirrorLook:
                    return "morning.notice.mirror";
                case RoomNotice::WallClockTime:
                    return "morning.notice.wall_clock";
                case RoomNotice::NothingHappens:
                    return "morning.notice.nothing";
                case RoomNotice::None:
                    break;
            }

            return {};
        }

        std::size_t partnerOf(std::size_t player)
        {
            return player == 0 ? 1 : 0;
        }

        void describeMugs(const RoomState& before, const RoomState& after, std::size_t player,
            std::vector<std::string_view>& lines)
        {
            for (std::size_t mug = 0; mug < after.mugTakenBy.size(); ++mug)
            {
                if (before.mugTakenBy[mug] != k_nobody || after.mugTakenBy[mug] == k_nobody)
                {
                    continue;
                }

                lines.push_back(after.mugTakenBy[mug] == player ? "morning.mug.self"
                    : "morning.mug.partner");
            }
        }

        void describePlayers(const RoomState& before, const RoomState& after, std::size_t player,
            std::vector<std::string_view>& lines)
        {
            const std::size_t partner = partnerOf(player);

            if (before.lockerOpen[player] == 0.0f && after.lockerOpen[player] > 0.0f)
            {
                lines.push_back("morning.locker.open");
            }

            if (!before.hasBadge[player] && after.hasBadge[player])
            {
                lines.push_back("morning.gear.self");
            }

            if (!before.hasBadge[partner] && after.hasBadge[partner])
            {
                lines.push_back("morning.gear.partner");
            }

            if (before.noticeCount[player] != after.noticeCount[player] && after.notice[player] != RoomNotice::None)
            {
                lines.push_back(noticeId(after.notice[player]));
            }
        }
    } //namespace

    RoomObject roomObjectFor(std::string_view name)
    {
        for (const auto& [objectName, object] : k_objectNames)
        {
            if (objectName == name)
            {
                return object;
            }
        }

        return RoomObject::Other;
    }

    const char* playerName(std::size_t player)
    {
        return player == 0 ? "Player1" : "Player2";
    }

    const char* toString(RoomAction action)
    {
        switch (action)
        {
            case RoomAction::SilenceAlarm:
                return "silence alarm";
            case RoomAction::StartBrew:
                return "start brew";
            case RoomAction::TakeMug:
                return "take mug";
            case RoomAction::OpenLocker:
                return "open locker";
            case RoomAction::CloseLocker:
                return "close locker";
            case RoomAction::TakeGear:
                return "take badge and sidearm";
            case RoomAction::ToggleRadio:
                return "toggle radio";
            case RoomAction::UseMirror:
                return "use mirror";
            case RoomAction::OpenFrontDoor:
                return "open front door";
            case RoomAction::OpenStreetDoor:
                return "open street door";
            case RoomAction::CloseStreetDoor:
                return "close street door";
            case RoomAction::PickUpToy:
                return "pick up toy";
            case RoomAction::ThrowToy:
                return "throw toy";
            case RoomAction::DropToy:
                return "drop toy";
            case RoomAction::StartDrive:
                return "start drive";
            case RoomAction::Inspect:
                return "inspect";
            case RoomAction::Refused:
                return "refused";
        }

        return "?";
    }

    const char* toString(RoomObject object)
    {
        if (object == RoomObject::Toy)
        {
            return "Toy";
        }

        for (const auto& [objectName, named] : k_objectNames)
        {
            if (named == object)
            {
                return objectName.data();
            }
        }

        return "other";
    }

    const char* promptText(const RoomState& state, RoomObject object, std::size_t player)
    {
        switch (object)
        {
            case RoomObject::AlarmClock:
                return state.isAlarmRinging ? "F: Silence the alarm" : "F: Alarm clock";
            case RoomObject::CoffeeMachine:
                return state.coffee == CoffeePhase::Idle ? "F: Brew coffee"
                    : state.coffee == CoffeePhase::Brewing ? "Brewing..." : "F: Take coffee";
            case RoomObject::MugA:
            case RoomObject::MugB:
                return state.coffee == CoffeePhase::Ready ? "F: Take coffee" : "F: Mug";
            case RoomObject::Radio:
                return state.isRadioOn ? "F: Turn the radio off" : "F: Turn the radio on";
            case RoomObject::Mirror:
                return "F: Look in the mirror";
            case RoomObject::Locker1:
            case RoomObject::Locker2:
            {
                const std::size_t owner = object == RoomObject::Locker1 ? 0 : 1;
                const bool isOwn = owner == player;

                if (state.isLockerOpen[owner])
                {
                    return isOwn ? "F: Close your locker" : "F: Close your flatmate's locker";
                }

                return isOwn ? "F: Open your locker" : "F: Open your flatmate's locker";
            }
            case RoomObject::Gear1:
            case RoomObject::Gear2:
            {
                const std::size_t owner = object == RoomObject::Gear1 ? 0 : 1;

                return owner == player ? "F: Take badge and sidearm" : "Your flatmate's gear";
            }
            case RoomObject::FrontDoor:
                return state.door == DoorPhase::Closed ? "F: Head out" : "";
            case RoomObject::WallClock:
                return "F: Check the time";
            case RoomObject::StreetDoor:
                return state.isStreetDoorOpen ? "F: Close the street door" : "F: Open the street door";
            case RoomObject::Toy:
            case RoomObject::Other:
                break;
        }

        return "F: Inspect";
    }

    std::vector<std::string_view> describeRoomChanges(const RoomState& before, const RoomState& after, std::size_t player)
    {
        std::vector<std::string_view> lines;

        if (before.isAlarmRinging && !after.isAlarmRinging)
        {
            lines.push_back(after.alarmSilencedBy == player ? "morning.alarm.self"
                : "morning.alarm.partner");
        }

        if (before.coffee != after.coffee)
        {
            lines.push_back(after.coffee == CoffeePhase::Brewing ? "morning.coffee.brewing"
                : "morning.coffee.ready");
        }

        describeMugs(before, after, player, lines);

        if (before.isRadioOn != after.isRadioOn)
        {
            lines.push_back(after.isRadioOn ? "morning.radio.on" : "morning.radio.off");
        }

        describePlayers(before, after, player, lines);

        if (before.door == DoorPhase::Closed && after.door != DoorPhase::Closed)
        {
            lines.push_back("morning.front_door.open");
        }

        describeStreetDoor(before, after, player, lines);
        describePhase(before, after, lines);

        return lines;
    }

    std::array<std::byte, k_roomStateBytes> encodeRoomState(const RoomState& state)
    {
        std::array<std::byte, k_roomStateBytes> bytes{};
        bytes[at(Offset::Tag)] = k_roomTag;
        bytes[at(Offset::Flags)] = (state.isAlarmRinging ? k_alarmRingingBit : std::byte{0})
            | (state.isRadioOn ? k_radioOnBit : std::byte{0})
            | (state.isStreetDoorOpen ? k_streetDoorOpenBit : std::byte{0});
        bytes[at(Offset::AlarmSilencedBy)] = static_cast<std::byte>(state.alarmSilencedBy);
        bytes[at(Offset::Coffee)] = static_cast<std::byte>(state.coffee);
        bytes[at(Offset::MugA)] = static_cast<std::byte>(state.mugTakenBy[0]);
        bytes[at(Offset::MugB)] = static_cast<std::byte>(state.mugTakenBy[1]);
        bytes[at(Offset::Locker1)] = quantize(state.lockerOpen[0]);
        bytes[at(Offset::Locker2)] = quantize(state.lockerOpen[1]);
        bytes[at(Offset::PlayerBits)] = static_cast<std::byte>(pairBits(state.hasBadge, k_badgeShift)
            | pairBits(state.hasSidearm, k_sidearmShift) | pairBits(state.hasUsedMirror, k_mirrorShift)
            | pairBits(state.isLockerOpen, k_lockerOpenShift));
        bytes[at(Offset::Door)] = static_cast<std::byte>(state.door);
        bytes[at(Offset::DoorOpen)] = quantize(state.doorOpen);
        bytes[at(Offset::NoticeCount1)] = static_cast<std::byte>(state.noticeCount[0]);
        bytes[at(Offset::NoticeCount2)] = static_cast<std::byte>(state.noticeCount[1]);
        bytes[at(Offset::Notice1)] = static_cast<std::byte>(state.notice[0]);
        bytes[at(Offset::Notice2)] = static_cast<std::byte>(state.notice[1]);
        bytes[at(Offset::Phase)] = static_cast<std::byte>(state.phase);
        bytes[at(Offset::StreetDoorOpen)] = quantize(state.streetDoorOpen);
        bytes[at(Offset::StreetDoorBy)] = static_cast<std::byte>(state.streetDoorBy);
        bytes[at(Offset::HeldToy1)] = static_cast<std::byte>(state.heldToy[0]);
        bytes[at(Offset::HeldToy2)] = static_cast<std::byte>(state.heldToy[1]);

        return bytes;
    }

    std::optional<RoomState> decodeRoomState(std::span<const std::byte> bytes)
    {
        if (bytes.size() != k_roomStateBytes || bytes[at(Offset::Tag)] != k_roomTag)
        {
            return std::nullopt;
        }

        const auto byteAt = [bytes](Offset offset) { return static_cast<uint8_t>(bytes[at(offset)]); };
        const uint8_t silencedBy = byteAt(Offset::AlarmSilencedBy);
        const uint8_t coffee = byteAt(Offset::Coffee);
        const uint8_t door = byteAt(Offset::Door);
        const uint8_t lastNotice = static_cast<uint8_t>(RoomNotice::NothingHappens);

        if (!validPlayer(silencedBy) || !validPlayer(byteAt(Offset::MugA)) || !validPlayer(byteAt(Offset::MugB))
            || coffee > static_cast<uint8_t>(CoffeePhase::Ready) || door > static_cast<uint8_t>(DoorPhase::Open)
            || byteAt(Offset::Notice1) > lastNotice || byteAt(Offset::Notice2) > lastNotice
            || byteAt(Offset::Phase) > static_cast<uint8_t>(MorningPhase::MorningComplete)
            || !validPlayer(byteAt(Offset::StreetDoorBy)) || !validToy(byteAt(Offset::HeldToy1))
            || !validToy(byteAt(Offset::HeldToy2)))
        {
            return std::nullopt;
        }

        const uint8_t playerBits = byteAt(Offset::PlayerBits);
        RoomState state{};
        state.isAlarmRinging = (bytes[at(Offset::Flags)] & k_alarmRingingBit) != std::byte{0};
        state.isRadioOn = (bytes[at(Offset::Flags)] & k_radioOnBit) != std::byte{0};
        state.isStreetDoorOpen = (bytes[at(Offset::Flags)] & k_streetDoorOpenBit) != std::byte{0};
        state.alarmSilencedBy = silencedBy;
        state.coffee = static_cast<CoffeePhase>(coffee);
        state.mugTakenBy = {byteAt(Offset::MugA), byteAt(Offset::MugB)};
        state.lockerOpen = {dequantize(bytes[at(Offset::Locker1)]), dequantize(bytes[at(Offset::Locker2)])};
        state.hasBadge = readPairBits(playerBits, k_badgeShift);
        state.hasSidearm = readPairBits(playerBits, k_sidearmShift);
        state.hasUsedMirror = readPairBits(playerBits, k_mirrorShift);
        state.isLockerOpen = readPairBits(playerBits, k_lockerOpenShift);
        state.door = static_cast<DoorPhase>(door);
        state.doorOpen = dequantize(bytes[at(Offset::DoorOpen)]);
        state.noticeCount = {byteAt(Offset::NoticeCount1), byteAt(Offset::NoticeCount2)};
        state.notice = {static_cast<RoomNotice>(byteAt(Offset::Notice1)),
            static_cast<RoomNotice>(byteAt(Offset::Notice2))};
        state.phase = static_cast<MorningPhase>(byteAt(Offset::Phase));
        state.streetDoorOpen = dequantize(bytes[at(Offset::StreetDoorOpen)]);
        state.streetDoorBy = byteAt(Offset::StreetDoorBy);
        state.heldToy = {byteAt(Offset::HeldToy1), byteAt(Offset::HeldToy2)};

        return state;
    }
} //namespace game
