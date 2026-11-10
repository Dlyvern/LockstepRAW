#include "Weapons/ReloadCues.hpp"

#include <algorithm>

namespace game
{
    namespace
    {
        uint8_t bitOf(ReloadCue cue)
        {
            return static_cast<uint8_t>(1u << static_cast<uint32_t>(cue));
        }

        void reach(ReloadCues& cues, std::size_t player, ReloadCue cue, float at)
        {
            PlayerReload& reload = cues.players[player];

            if (at < 0.0f || (reload.cues & bitOf(cue)) != 0 || reloadClipSeconds(reload) < at)
            {
                return;
            }

            reload.cues |= bitOf(cue);
            cues.events.push_back(ReloadEvent{.player = player, .cue = cue});
        }
    } //namespace

    const ReloadTimeline& reloadTimeline(ReloadKind kind)
    {
        return kind == ReloadKind::Empty ? k_emptyReloadTimeline : k_tacticalReloadTimeline;
    }

    void updateReloadCues(ReloadCues& cues, const PlayerStatuses& statuses, const AmmoSettings& ammo, float seconds)
    {
        cues.events.clear();

        for (std::size_t player = 0; player < k_playerCount; ++player)
        {
            const PlayerStatus& status = statuses[player];
            PlayerReload& reload = cues.players[player];

            if (!status.isReloading)
            {
                reload.isActive = false;
                continue;
            }

            if (!reload.isActive)
            {
                const ReloadKind kind = status.magazine <= 0 ? ReloadKind::Empty : ReloadKind::Tactical;
                reload = PlayerReload{.isActive = true,
                    .kind = kind,
                    .length = std::max(kind == ReloadKind::Empty ? ammo.emptyReloadSeconds : ammo.reloadSeconds, 0.1f)};
            }
            else
            {
                reload.seconds += seconds;
            }

            const ReloadTimeline& timeline = reloadTimeline(reload.kind);
            reach(cues, player, ReloadCue::MagazineOut, timeline.magazineOut);
            reach(cues, player, ReloadCue::MagazineIn, timeline.magazineIn);
            reach(cues, player, ReloadCue::Charge, timeline.charge);
        }
    }

    float reloadClipSeconds(const PlayerReload& reload)
    {
        return reload.seconds * reloadTimeline(reload.kind).length / std::max(reload.length, 0.1f);
    }
} //namespace game
