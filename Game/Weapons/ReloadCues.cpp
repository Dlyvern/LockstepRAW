#include "Weapons/ReloadCues.hpp"

#include <algorithm>

namespace game
{
    namespace
    {
        constexpr ReloadTimeline k_m4Tactical{.length = 3.633f, .magazineOut = 1.0f, .magazineIn = 2.033f};
        constexpr ReloadTimeline k_m4Empty{.length = 3.8f, .magazineOut = 1.033f, .magazineIn = 2.067f, .charge = 2.667f};
        constexpr ReloadTimeline k_glockTactical{.length = 2.8f, .magazineOut = 0.833f, .magazineIn = 1.467f};
        constexpr ReloadTimeline k_glockEmpty{.length = 3.033f, .magazineOut = 0.833f, .magazineIn = 1.467f,
            .charge = 1.9f};

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

    const ReloadTimeline& reloadTimeline(WeaponKind weapon, ReloadKind kind)
    {
        const bool isEmpty = kind == ReloadKind::Empty;

        switch (weapon)
        {
            case WeaponKind::M4:
            {
                return isEmpty ? k_m4Empty : k_m4Tactical;
            }
            case WeaponKind::Glock:
            {
                return isEmpty ? k_glockEmpty : k_glockTactical;
            }
            case WeaponKind::Kriss:
            case WeaponKind::Count:
            {
                break;
            }
        }

        return isEmpty ? k_emptyReloadTimeline : k_tacticalReloadTimeline;
    }

    void updateReloadCues(ReloadCues& cues, const PlayerStatuses& statuses, const WeaponStatsTable& weapons,
        float seconds)
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
                const AmmoSettings& ammo = weaponStats(weapons, status.weapon).ammo;
                reload = PlayerReload{.isActive = true,
                    .weapon = status.weapon,
                    .kind = kind,
                    .length = std::max(kind == ReloadKind::Empty ? ammo.emptyReloadSeconds : ammo.reloadSeconds, 0.1f)};
            }
            else
            {
                reload.seconds += seconds;
            }

            const ReloadTimeline& timeline = reloadTimeline(reload.weapon, reload.kind);
            reach(cues, player, ReloadCue::MagazineOut, timeline.magazineOut);
            reach(cues, player, ReloadCue::MagazineIn, timeline.magazineIn);
            reach(cues, player, ReloadCue::Charge, timeline.charge);
        }
    }

    float reloadClipSeconds(const PlayerReload& reload)
    {
        return reload.seconds * reloadTimeline(reload.weapon, reload.kind).length / std::max(reload.length, 0.1f);
    }
} //namespace game
