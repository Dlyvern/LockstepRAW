#include "Station/StationState.hpp"

namespace game
{
    namespace
    {
        constexpr std::byte k_stationTag{0x53};

        enum class Offset : std::size_t
        {
            Tag,
            Phase,
            BriefedBy,
            Gear1,
            Gear2,
            TargetsHit1,
            TargetsHit2,
            TargetsDown,
            NoticeCount1,
            NoticeCount2,
            Notice1,
            Notice2
        };

        constexpr std::size_t at(Offset offset)
        {
            return static_cast<std::size_t>(offset);
        }

        std::size_t partnerOf(std::size_t player)
        {
            return player == 0 ? 1 : 0;
        }

        std::string_view noticeId(StationNotice notice)
        {
            switch (notice)
            {
                case StationNotice::ReportFirst:
                    return "station.notice.report_first";
                case StationNotice::AlreadyTaken:
                    return "station.notice.already_taken";
                case StationNotice::AlreadyBriefed:
                    return "station.notice.already_briefed";
                case StationNotice::ChiefTalking:
                    return "station.notice.chief_talking";
                case StationNotice::NothingHappens:
                    return "station.notice.nothing";
                case StationNotice::WaitForPartner:
                    return "station.notice.wait_partner";
                case StationNotice::None:
                    break;
            }

            return {};
        }

        bool gained(uint8_t before, uint8_t after, ArmoryItem item)
        {
            const auto bit = static_cast<uint8_t>(item);
            return (before & bit) == 0 && (after & bit) != 0;
        }

        void describeGear(const StationState& before, const StationState& after, std::size_t player,
            StationChanges& changes)
        {
            const uint8_t own = before.gear[player];
            const uint8_t ownAfter = after.gear[player];

            if (gained(own, ownAfter, ArmoryItem::Armor))
            {
                changes.voices.push_back(StationVoice::GiveArmor);
            }

            if (gained(own, ownAfter, ArmoryItem::Weapon))
            {
                changes.voices.push_back(StationVoice::GiveWeapon);
            }

            if (gained(own, ownAfter, ArmoryItem::Ammo))
            {
                changes.voices.push_back(StationVoice::GiveAmmo);
            }

            if (own != k_allArmoryItems && ownAfter == k_allArmoryItems)
            {
                changes.lines.push_back("station.tip.range");
            }

            const std::size_t partner = partnerOf(player);

            if (before.gear[partner] != k_allArmoryItems && after.gear[partner] == k_allArmoryItems
                && after.phase == StationPhase::Armory)
            {
                changes.lines.push_back("station.narration.partner_geared");
            }
        }

        void describePhase(const StationState& before, const StationState& after, StationChanges& changes)
        {
            if (before.phase == after.phase)
            {
                return;
            }

            if (before.phase == StationPhase::Briefing)
            {
                changes.voices.push_back(StationVoice::BriefingOver);
            }

            switch (after.phase)
            {
                case StationPhase::Briefing:
                    changes.voices.push_back(StationVoice::Briefing);
                    break;
                case StationPhase::Armory:
                    changes.lines.push_back("station.narration.armory");
                    break;
                case StationPhase::GoToCar:
                    changes.lines.push_back("station.narration.geared");
                    break;
                case StationPhase::MissionStart:
                    changes.lines.push_back("station.narration.drive");
                    break;
                case StationPhase::Report:
                    break;
            }
        }
    } //namespace

    bool hasArmoryItem(const StationState& state, std::size_t player, ArmoryItem item)
    {
        return (state.gear[player] & static_cast<uint8_t>(item)) != 0;
    }

    const char* toString(StationPhase phase)
    {
        switch (phase)
        {
            case StationPhase::Report:
                return "report to the chief";
            case StationPhase::Briefing:
                return "briefing";
            case StationPhase::Armory:
                return "armory";
            case StationPhase::GoToCar:
                return "go to the car";
            case StationPhase::MissionStart:
                return "mission start";
        }

        return "?";
    }

    StationChanges describeStationChanges(const StationState& before, const StationState& after, std::size_t player,
        uint8_t targetMask)
    {
        StationChanges changes{};
        describePhase(before, after, changes);
        describeGear(before, after, player, changes);

        const bool wasQualified = (before.targetsHit[player] & targetMask) == targetMask;
        const bool isQualified = (after.targetsHit[player] & targetMask) == targetMask;

        if (targetMask != 0 && isQualified && !wasQualified)
        {
            changes.voices.push_back(StationVoice::Qualified);
        }

        if (before.noticeCount[player] != after.noticeCount[player] && after.notice[player] != StationNotice::None)
        {
            changes.lines.push_back(noticeId(after.notice[player]));
        }

        return changes;
    }

    std::array<std::byte, k_stationStateBytes> encodeStationState(const StationState& state)
    {
        std::array<std::byte, k_stationStateBytes> bytes{};
        bytes[at(Offset::Tag)] = k_stationTag;
        bytes[at(Offset::Phase)] = static_cast<std::byte>(state.phase);
        bytes[at(Offset::BriefedBy)] = static_cast<std::byte>(state.briefedBy);
        bytes[at(Offset::Gear1)] = static_cast<std::byte>(state.gear[0]);
        bytes[at(Offset::Gear2)] = static_cast<std::byte>(state.gear[1]);
        bytes[at(Offset::TargetsHit1)] = static_cast<std::byte>(state.targetsHit[0]);
        bytes[at(Offset::TargetsHit2)] = static_cast<std::byte>(state.targetsHit[1]);
        bytes[at(Offset::TargetsDown)] = static_cast<std::byte>(state.targetsDown);
        bytes[at(Offset::NoticeCount1)] = static_cast<std::byte>(state.noticeCount[0]);
        bytes[at(Offset::NoticeCount2)] = static_cast<std::byte>(state.noticeCount[1]);
        bytes[at(Offset::Notice1)] = static_cast<std::byte>(state.notice[0]);
        bytes[at(Offset::Notice2)] = static_cast<std::byte>(state.notice[1]);

        return bytes;
    }

    std::optional<StationState> decodeStationState(std::span<const std::byte> bytes)
    {
        if (bytes.size() != k_stationStateBytes || bytes[at(Offset::Tag)] != k_stationTag)
        {
            return std::nullopt;
        }

        const auto byteAt = [bytes](Offset offset) { return static_cast<uint8_t>(bytes[at(offset)]); };
        const uint8_t lastNotice = static_cast<uint8_t>(StationNotice::WaitForPartner);
        const uint8_t briefedBy = byteAt(Offset::BriefedBy);

        if (byteAt(Offset::Phase) > static_cast<uint8_t>(StationPhase::MissionStart)
            || (briefedBy != k_nobody && briefedBy >= k_playerCount) || byteAt(Offset::Gear1) > k_allArmoryItems
            || byteAt(Offset::Gear2) > k_allArmoryItems || byteAt(Offset::Notice1) > lastNotice
            || byteAt(Offset::Notice2) > lastNotice)
        {
            return std::nullopt;
        }

        StationState state{};
        state.phase = static_cast<StationPhase>(byteAt(Offset::Phase));
        state.briefedBy = briefedBy;
        state.gear = {byteAt(Offset::Gear1), byteAt(Offset::Gear2)};
        state.targetsHit = {byteAt(Offset::TargetsHit1), byteAt(Offset::TargetsHit2)};
        state.targetsDown = byteAt(Offset::TargetsDown);
        state.noticeCount = {byteAt(Offset::NoticeCount1), byteAt(Offset::NoticeCount2)};
        state.notice = {static_cast<StationNotice>(byteAt(Offset::Notice1)),
            static_cast<StationNotice>(byteAt(Offset::Notice2))};

        return state;
    }
} //namespace game
