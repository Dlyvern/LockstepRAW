#ifndef LOCKSTEP_GAME_WEAPONS_WEAPON_CATALOG_HPP
#define LOCKSTEP_GAME_WEAPONS_WEAPON_CATALOG_HPP

#include "Weapons/Ammo.hpp"
#include "Weapons/FireModes.hpp"
#include "Weapons/Penetration.hpp"
#include "Weapons/Recoil.hpp"
#include "Weapons/ShotAim.hpp"
#include "Weapons/WeaponKind.hpp"

#include <array>

namespace game
{
    // Everything that makes one weapon of the loadout handle differently. Host rules (ammo, fire
    // rate, spread, penetration, damage, switch time) and the client's prediction read the same
    // values. Editable per weapon from the dev panel.
    // ammo:        magazine size, reserve and the reload times (matching its reload clips).
    // fire:        cyclic rate and burst rules.
    // recoil:      the camera recoil pattern.
    // spread:      shot cone and sight offset limit.
    // penetration: what its rounds go through.
    // damage:      damage multiplier of a hit, relative to the KRISS' .45.
    // modes:       per FireMode, whether B may select it.
    // holsterSeconds, drawSeconds: Player seconds to put it away and to bring it up (a switch takes
    //              the old weapon's holster plus the new one's draw; no shots meanwhile).
    // flashScale:  size of its muzzle flash relative to the KRISS'.
    struct WeaponStats
    {
        AmmoSettings ammo{};
        FireModeSettings fire{};
        RecoilSettings recoil{};
        SpreadSettings spread{};
        PenetrationSettings penetration{};
        float damage{1.0f};
        std::array<bool, 3> modes{{true, true, true}};
        float holsterSeconds{0.35f};
        float drawSeconds{0.5f};
        float flashScale{1.0f};
    };

    // The per-weapon stats of the whole loadout, indexed by weaponIndex().
    using WeaponStatsTable = std::array<WeaponStats, k_weaponCount>;

    // Returns: the default stats: the M4 (5.56, automatic or semi, hard-hitting, strong recoil, goes
    //          through more), the Glock 17 (9 mm, semi only, quick to draw, weaker) and the KRISS
    //          (.45, semi, burst or automatic).
    [[nodiscard]] WeaponStatsTable defaultWeaponStats();

    // stats: the loadout's stats.
    // kind:  a weapon.
    // Returns: its stats.
    [[nodiscard]] const WeaponStats& weaponStats(const WeaponStatsTable& stats, WeaponKind kind);

    // The mutable version of weaponStats().
    [[nodiscard]] WeaponStats& weaponStats(WeaponStatsTable& stats, WeaponKind kind);

    // stats: a weapon's stats.
    // mode:  a fire mode.
    // Returns: the next fire mode B selects on this weapon (mode itself if it has only one).
    [[nodiscard]] FireMode nextWeaponFireMode(const WeaponStats& stats, FireMode mode);

    // stats: a weapon's stats.
    // mode:  the fire mode in use.
    // Returns: `mode` if the weapon has it, else the weapon's first mode from Auto down to Semi.
    [[nodiscard]] FireMode allowedFireMode(const WeaponStats& stats, FireMode mode);

    // stats: the loadout's stats.
    // from:  the weapon put away.
    // to:    the weapon brought up.
    // Returns: Player seconds the switch takes.
    [[nodiscard]] float weaponSwitchSeconds(const WeaponStatsTable& stats, WeaponKind from, WeaponKind to);

#if LOCKSTEP_DEV_TOOLS
    // Draws one weapon's tuning in the dev panel: fire rate and modes, camera recoil, spread,
    // penetration, ammo and reload times, damage, switch times and flash size.
    // stats: the weapon's stats.
    void weaponStatsDevSection(WeaponStats& stats);
#endif
} //namespace game

#endif //LOCKSTEP_GAME_WEAPONS_WEAPON_CATALOG_HPP
