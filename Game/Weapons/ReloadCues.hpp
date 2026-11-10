#ifndef LOCKSTEP_GAME_WEAPONS_RELOAD_CUES_HPP
#define LOCKSTEP_GAME_WEAPONS_RELOAD_CUES_HPP

#include "Player/PlayerStatus.hpp"
#include "Weapons/Ammo.hpp"

#include <array>
#include <cstdint>
#include <vector>

namespace game
{
    // Which reload runs: with rounds left (the chamber stays loaded) or from an empty magazine
    // (the charging handle is pulled as well, and it takes longer).
    enum class ReloadKind : uint8_t
    {
        Tactical = 0,
        Empty
    };

    // The moments of a reload, in seconds of its first-person clip (ReloadTactical/ReloadEmpty of
    // characters/swat_fp_kriss, authored by Content/source/scripts/fp_reload.py). The clip and
    // these cues are stretched to the host's reload time (AmmoSettings).
    // length:         the clip's length.
    // magazineOut:    the old magazine leaves the well (it is dropped here).
    // magazineIn:     the new magazine is seated.
    // charge:         the charging handle is pulled; negative for none.
    struct ReloadTimeline
    {
        float length{2.0f};
        float magazineOut{0.30f};
        float magazineIn{1.15f};
        float charge{-1.0f};
    };

    inline constexpr ReloadTimeline k_tacticalReloadTimeline{.length = 2.0f, .magazineOut = 0.30f, .magazineIn = 1.15f};
    inline constexpr ReloadTimeline k_emptyReloadTimeline{.length = 2.6f, .magazineOut = 0.30f, .magazineIn = 1.20f,
        .charge = 1.75f};

    // kind:    a reload kind.
    // Returns: its timeline.
    [[nodiscard]] const ReloadTimeline& reloadTimeline(ReloadKind kind);

    // A moment of a reload this frame reached.
    enum class ReloadCue : uint8_t
    {
        MagazineOut = 0,
        MagazineIn,
        Charge
    };

    // One reload cue reached this frame.
    // player: the player reloading.
    // cue:    the moment.
    struct ReloadEvent
    {
        std::size_t player{0};
        ReloadCue cue{ReloadCue::MagazineOut};
    };

    // A player's running reload as this machine shows it (from the replicated status).
    // isActive: a reload runs.
    // kind:     which one (the magazine was empty when it started).
    // seconds:  Player seconds since it started.
    // length:   Player seconds it takes on the host.
    // cues:     the cues already reached (bit per ReloadCue).
    struct PlayerReload
    {
        bool isActive{false};
        ReloadKind kind{ReloadKind::Tactical};
        float seconds{0.0f};
        float length{2.0f};
        uint8_t cues{0};
    };

    // Both players' reloads and this frame's cues. Presentation only.
    // players: per player.
    // events:  the cues reached by updateReloadCues() this frame.
    struct ReloadCues
    {
        std::array<PlayerReload, k_playerCount> players{};
        std::vector<ReloadEvent> events{};
    };

    // Follows the players' reloads (a status turning isReloading on starts one; off ends it) and
    // collects the cues reached this frame.
    // cues:     the reload state, updated.
    // statuses: both players' statuses.
    // ammo:     the reload times.
    // seconds:  Player time since the previous frame.
    void updateReloadCues(ReloadCues& cues, const PlayerStatuses& statuses, const AmmoSettings& ammo, float seconds);

    // reload:  a running reload.
    // Returns: how far its clip is, in clip seconds (the reload's time stretched to the timeline).
    [[nodiscard]] float reloadClipSeconds(const PlayerReload& reload);
} //namespace game

#endif //LOCKSTEP_GAME_WEAPONS_RELOAD_CUES_HPP
