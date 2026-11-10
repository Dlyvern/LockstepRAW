#include "Weapons/Loadout.hpp"

namespace game
{
    void issueLoadout(Pawn& pawn, const WeaponStatsTable& weapons)
    {
        for (std::size_t index = 0; index < k_weaponCount; ++index)
        {
            const auto kind = static_cast<WeaponKind>(index);
            Ammo& ammo = kind == pawn.weapon ? pawn.ammo : pawn.stowedAmmo[index];
            issueAmmo(ammo, weaponStats(weapons, kind).ammo);
            ammo.isIssued = true;
        }
    }

    void refillLoadout(Pawn& pawn, const WeaponStatsTable& weapons)
    {
        for (std::size_t index = 0; index < k_weaponCount; ++index)
        {
            const auto kind = static_cast<WeaponKind>(index);
            refillReserve(kind == pawn.weapon ? pawn.ammo : pawn.stowedAmmo[index], weaponStats(weapons, kind).ammo);
        }
    }

    void switchWeapon(Pawn& pawn, const WeaponStatsTable& weapons, WeaponKind weapon)
    {
        if (weapon == pawn.weapon)
        {
            return;
        }

        const uint8_t processedReloads = pawn.ammo.processedReloads;
        Ammo stowed = pawn.ammo;
        stowed.reloadSeconds = 0.0f;
        pawn.stowedAmmo[weaponIndex(pawn.weapon)] = stowed;
        pawn.switchSeconds = weaponSwitchSeconds(weapons, pawn.weapon, weapon);
        pawn.weapon = weapon;
        pawn.ammo = pawn.stowedAmmo[weaponIndex(weapon)];
        pawn.ammo.processedReloads = processedReloads;
        pawn.ammo.reloadSeconds = 0.0f;
        pawn.ammo.isIssued = true;
    }
} //namespace game
