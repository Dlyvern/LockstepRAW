#ifndef LOCKSTEP_GAME_STATION_STATION_STATE_HPP
#define LOCKSTEP_GAME_STATION_STATION_STATE_HPP

#include "Level/RoomState.hpp"
#include "Player/Pawns.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

namespace game
{
    // Most range targets a station level may have (StationState keeps one bit per target).
    inline constexpr std::size_t k_maxRangeTargets{8};

    // The steps of the station mission, in order. The range is optional: its targets work in every
    // phase but never hold the mission up.
    // Report:       both players go up to the chief's office and one reports (E on
    //               Interact_Briefing).
    // Briefing:     the chief talks; the host waits out the briefing's subtitles.
    // Armory:       each player takes armor, weapon and ammo at the basement counter.
    // GoToCar:      both players walk to the car in the lot.
    // MissionStart: both are at the car; the screen fades out (end of level 1 for now).
    enum class StationPhase : uint8_t
    {
        Report,
        Briefing,
        Armory,
        GoToCar,
        MissionStart
    };

    // What the armory counter hands out, as bits of StationState::gear.
    enum class ArmoryItem : uint8_t
    {
        Armor = 0x01,
        Weapon = 0x02,
        Ammo = 0x04
    };

    // StationState::gear with every armory item.
    inline constexpr uint8_t k_allArmoryItems{0x07};

    // Why the station refused a player, or what it tells them (shown once per change).
    enum class StationNotice : uint8_t
    {
        None,
        ReportFirst,
        AlreadyTaken,
        AlreadyBriefed,
        ChiefTalking,
        NothingHappens,
        WaitForPartner
    };

    // Lines and voices the local player gets when the station state changes (or, for RangeIntro,
    // when they walk up to the range).
    // Briefing:      the chief's briefing starts.
    // BriefingOver:  the briefing ended (or was skipped); its subtitles stop.
    // GiveArmor/GiveWeapon/GiveAmmo: the armorer hands the local player an item.
    // RangeIntro:    the range officer's introduction.
    // Qualified:     the local player knocked down every target.
    enum class StationVoice : uint8_t
    {
        Briefing,
        BriefingOver,
        GiveArmor,
        GiveWeapon,
        GiveAmmo,
        RangeIntro,
        Qualified
    };

    // The shared state of the station mission. The host owns it and sends it to the client
    // whenever it changes (encodeStationState()).
    // phase:       the mission step.
    // briefedBy:   the player who reported to the chief, k_nobody before.
    // gear:        per player, the ArmoryItem bits taken.
    // targetsHit:  per player, one bit per range target they knocked down.
    // targetsDown: one bit per range target lying down.
    // noticeCount: per player, counts notices so a repeated one shows again.
    // notice:      per player, the newest notice.
    struct StationState
    {
        StationPhase phase{StationPhase::Report};
        uint8_t briefedBy{k_nobody};
        std::array<uint8_t, k_playerCount> gear{};
        std::array<uint8_t, k_playerCount> targetsHit{};
        uint8_t targetsDown{0};
        std::array<uint8_t, k_playerCount> noticeCount{};
        std::array<StationNotice, k_playerCount> notice{};

        friend bool operator==(const StationState&, const StationState&) = default;
    };

    // Size of an encoded StationState message.
    inline constexpr std::size_t k_stationStateBytes{12};

    // Changes the local player is told about.
    // lines:  dialogue ids of narration lines ("station.narration.*", "station.notice.*", ...),
    //         in order.
    // voices: dialogue to queue, in order.
    struct StationChanges
    {
        std::vector<std::string_view> lines;
        std::vector<StationVoice> voices;
    };

    // state:   the station state.
    // player:  a player index.
    // item:    an armory item.
    // Returns: whether that player took it.
    [[nodiscard]] bool hasArmoryItem(const StationState& state, std::size_t player, ArmoryItem item);

    // Returns: a readable name of the phase, for logs and the dev panel.
    [[nodiscard]] const char* toString(StationPhase phase);

    // Lists what changed for the local player between two states.
    // before:      the state the player was last told about.
    // after:       the newest state.
    // player:      the local player's index.
    // targetMask:  bits of every range target in the level.
    // Returns:     the lines and voices.
    [[nodiscard]] StationChanges describeStationChanges(const StationState& before, const StationState& after,
        std::size_t player, uint8_t targetMask);

    // Encodes the state for the reliable Game message to the client.
    // state:   the state.
    // Returns: the message bytes.
    [[nodiscard]] std::array<std::byte, k_stationStateBytes> encodeStationState(const StationState& state);

    // Decodes a station state message.
    // bytes:   a received Game message.
    // Returns: the state, or nothing if the message is not a valid station state.
    [[nodiscard]] std::optional<StationState> decodeStationState(std::span<const std::byte> bytes);
} //namespace game

#endif //LOCKSTEP_GAME_STATION_STATION_STATE_HPP
