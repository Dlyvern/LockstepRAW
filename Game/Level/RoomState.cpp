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
        constexpr float k_quantization{255.0f};
        constexpr uint8_t k_badgeShift{0};
        constexpr uint8_t k_sidearmShift{2};
        constexpr uint8_t k_mirrorShift{4};
        constexpr uint8_t k_waitingShift{6};

        constexpr std::array<std::pair<std::string_view, RoomObject>, 10> k_objectNames{{
            {"Interact_AlarmClock", RoomObject::AlarmClock},
            {"Interact_CoffeeMachine", RoomObject::CoffeeMachine},
            {"Mug_A", RoomObject::MugA},
            {"Mug_B", RoomObject::MugB},
            {"Interact_Radio", RoomObject::Radio},
            {"Interact_Mirror", RoomObject::Mirror},
            {"Interact_Locker_1", RoomObject::Locker1},
            {"Interact_Locker_2", RoomObject::Locker2},
            {"Interact_FrontDoor", RoomObject::FrontDoor},
            {"Interact_WallClock", RoomObject::WallClock}}};

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
            Notice2
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

        const char* noticeText(RoomNotice notice)
        {
            switch (notice)
            {
                case RoomNotice::AlarmAlreadyOff:
                    return "The alarm is already off.";
                case RoomNotice::StillBrewing:
                    return "Still brewing. Smells good.";
                case RoomNotice::CoffeeNotReady:
                    return "Empty. Brew some coffee first.";
                case RoomNotice::AlreadyHaveCoffee:
                    return "You already have your coffee.";
                case RoomNotice::NotYourLocker:
                    return "That's your flatmate's locker.";
                case RoomNotice::LockerEmpty:
                    return "Your locker is empty now.";
                case RoomNotice::NeedGear:
                    return "Not without your badge and sidearm.";
                case RoomNotice::PartnerNeedsGear:
                    return "Your flatmate isn't geared up yet.";
                case RoomNotice::WaitingForPartner:
                    return "You wait at the door for your flatmate.";
                case RoomNotice::PartnerAbsent:
                    return "You never leave without your flatmate.";
                case RoomNotice::MirrorLook:
                    return "You look tired. Same as yesterday.";
                case RoomNotice::WallClockTime:
                    return "07:40. Shift starts at eight.";
                case RoomNotice::NothingHappens:
                    return "Nothing happens.";
                case RoomNotice::None:
                    break;
            }

            return "";
        }

        std::size_t partnerOf(std::size_t player)
        {
            return player == 0 ? 1 : 0;
        }

        void describeMugs(const RoomState& before, const RoomState& after, std::size_t player,
            std::vector<const char*>& lines)
        {
            for (std::size_t mug = 0; mug < after.mugTakenBy.size(); ++mug)
            {
                if (before.mugTakenBy[mug] != k_nobody || after.mugTakenBy[mug] == k_nobody)
                {
                    continue;
                }

                lines.push_back(after.mugTakenBy[mug] == player ? "You take a mug of coffee."
                    : "Your flatmate takes a mug of coffee.");
            }
        }

        void describePlayers(const RoomState& before, const RoomState& after, std::size_t player,
            std::vector<const char*>& lines)
        {
            const std::size_t partner = partnerOf(player);

            if (before.lockerOpen[player] == 0.0f && after.lockerOpen[player] > 0.0f)
            {
                lines.push_back("Your locker door swings open.");
            }

            if (!before.hasBadge[player] && after.hasBadge[player])
            {
                lines.push_back("You clip on your badge and holster your sidearm.");
            }

            if (!before.hasBadge[partner] && after.hasBadge[partner])
            {
                lines.push_back("Your flatmate is geared up.");
            }

            if (!before.isWaitingAtDoor[partner] && after.isWaitingAtDoor[partner] && after.door == DoorPhase::Closed)
            {
                lines.push_back("Your flatmate is waiting at the front door.");
            }

            if (before.noticeCount[player] != after.noticeCount[player] && after.notice[player] != RoomNotice::None)
            {
                lines.push_back(noticeText(after.notice[player]));
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
            case RoomAction::TakeGear:
                return "take badge and sidearm";
            case RoomAction::ToggleRadio:
                return "toggle radio";
            case RoomAction::UseMirror:
                return "use mirror";
            case RoomAction::ReadyAtDoor:
                return "ready at door";
            case RoomAction::Inspect:
                return "inspect";
            case RoomAction::Refused:
                return "refused";
        }

        return "?";
    }

    const char* toString(RoomObject object)
    {
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
                return state.isAlarmRinging ? "E: Silence the alarm" : "E: Alarm clock";
            case RoomObject::CoffeeMachine:
                return state.coffee == CoffeePhase::Idle ? "E: Brew coffee"
                    : state.coffee == CoffeePhase::Brewing ? "Brewing..." : "E: Take coffee";
            case RoomObject::MugA:
            case RoomObject::MugB:
                return state.coffee == CoffeePhase::Ready ? "E: Take coffee" : "E: Mug";
            case RoomObject::Radio:
                return state.isRadioOn ? "E: Turn the radio off" : "E: Turn the radio on";
            case RoomObject::Mirror:
                return "E: Look in the mirror";
            case RoomObject::Locker1:
            case RoomObject::Locker2:
            {
                const std::size_t owner = object == RoomObject::Locker1 ? 0 : 1;

                if (owner != player)
                {
                    return "Your flatmate's locker";
                }

                if (state.lockerOpen[owner] == 0.0f)
                {
                    return "E: Open your locker";
                }

                return state.hasBadge[owner] ? "Your locker" : "E: Take badge and sidearm";
            }
            case RoomObject::FrontDoor:
                return state.door == DoorPhase::Closed ? "E: Head out (together)" : "";
            case RoomObject::WallClock:
                return "E: Check the time";
            case RoomObject::Other:
                break;
        }

        return "E: Inspect";
    }

    std::vector<const char*> describeRoomChanges(const RoomState& before, const RoomState& after, std::size_t player)
    {
        std::vector<const char*> lines;

        if (before.isAlarmRinging && !after.isAlarmRinging)
        {
            lines.push_back(after.alarmSilencedBy == player ? "You silence the alarm."
                : "Your flatmate silenced the alarm before you got to it.");
        }

        if (before.coffee != after.coffee)
        {
            lines.push_back(after.coffee == CoffeePhase::Brewing ? "The coffee machine starts to gurgle."
                : "Coffee's ready. Two mugs on the counter.");
        }

        describeMugs(before, after, player, lines);

        if (before.isRadioOn != after.isRadioOn)
        {
            lines.push_back(after.isRadioOn ? "The radio crackles on." : "The radio goes quiet.");
        }

        describePlayers(before, after, player, lines);

        if (before.door == DoorPhase::Closed && after.door != DoorPhase::Closed)
        {
            lines.push_back("The front door swings open.");
        }

        if (before.door != DoorPhase::Open && after.door == DoorPhase::Open)
        {
            lines.push_back("Room complete.");
        }

        return lines;
    }

    std::array<std::byte, k_roomStateBytes> encodeRoomState(const RoomState& state)
    {
        std::array<std::byte, k_roomStateBytes> bytes{};
        bytes[at(Offset::Tag)] = k_roomTag;
        bytes[at(Offset::Flags)] = (state.isAlarmRinging ? k_alarmRingingBit : std::byte{0})
            | (state.isRadioOn ? k_radioOnBit : std::byte{0});
        bytes[at(Offset::AlarmSilencedBy)] = static_cast<std::byte>(state.alarmSilencedBy);
        bytes[at(Offset::Coffee)] = static_cast<std::byte>(state.coffee);
        bytes[at(Offset::MugA)] = static_cast<std::byte>(state.mugTakenBy[0]);
        bytes[at(Offset::MugB)] = static_cast<std::byte>(state.mugTakenBy[1]);
        bytes[at(Offset::Locker1)] = quantize(state.lockerOpen[0]);
        bytes[at(Offset::Locker2)] = quantize(state.lockerOpen[1]);
        bytes[at(Offset::PlayerBits)] = static_cast<std::byte>(pairBits(state.hasBadge, k_badgeShift)
            | pairBits(state.hasSidearm, k_sidearmShift) | pairBits(state.hasUsedMirror, k_mirrorShift)
            | pairBits(state.isWaitingAtDoor, k_waitingShift));
        bytes[at(Offset::Door)] = static_cast<std::byte>(state.door);
        bytes[at(Offset::DoorOpen)] = quantize(state.doorOpen);
        bytes[at(Offset::NoticeCount1)] = static_cast<std::byte>(state.noticeCount[0]);
        bytes[at(Offset::NoticeCount2)] = static_cast<std::byte>(state.noticeCount[1]);
        bytes[at(Offset::Notice1)] = static_cast<std::byte>(state.notice[0]);
        bytes[at(Offset::Notice2)] = static_cast<std::byte>(state.notice[1]);

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
            || byteAt(Offset::Notice1) > lastNotice || byteAt(Offset::Notice2) > lastNotice)
        {
            return std::nullopt;
        }

        const uint8_t playerBits = byteAt(Offset::PlayerBits);
        RoomState state{};
        state.isAlarmRinging = (bytes[at(Offset::Flags)] & k_alarmRingingBit) != std::byte{0};
        state.isRadioOn = (bytes[at(Offset::Flags)] & k_radioOnBit) != std::byte{0};
        state.alarmSilencedBy = silencedBy;
        state.coffee = static_cast<CoffeePhase>(coffee);
        state.mugTakenBy = {byteAt(Offset::MugA), byteAt(Offset::MugB)};
        state.lockerOpen = {dequantize(bytes[at(Offset::Locker1)]), dequantize(bytes[at(Offset::Locker2)])};
        state.hasBadge = readPairBits(playerBits, k_badgeShift);
        state.hasSidearm = readPairBits(playerBits, k_sidearmShift);
        state.hasUsedMirror = readPairBits(playerBits, k_mirrorShift);
        state.isWaitingAtDoor = readPairBits(playerBits, k_waitingShift);
        state.door = static_cast<DoorPhase>(door);
        state.doorOpen = dequantize(bytes[at(Offset::DoorOpen)]);
        state.noticeCount = {byteAt(Offset::NoticeCount1), byteAt(Offset::NoticeCount2)};
        state.notice = {static_cast<RoomNotice>(byteAt(Offset::Notice1)),
            static_cast<RoomNotice>(byteAt(Offset::Notice2))};

        return state;
    }
} //namespace game
