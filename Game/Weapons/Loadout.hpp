#ifndef LOCKSTEP_GAME_WEAPONS_LOADOUT_HPP
#define LOCKSTEP_GAME_WEAPONS_LOADOUT_HPP

#include "Player/Pawns.hpp"
#include "Weapons/WeaponCatalog.hpp"

namespace game
{
    // Host: hands out every loadout weapon's rounds (a full magazine and the issued reserve each).
    // pawn:    the armed pawn; `ammo` is its weapon in hand, `stowedAmmo` the others.
    // weapons: the loadout's stats.
    void issueLoadout(Pawn& pawn, const WeaponStatsTable& weapons);

    // Host: fills every loadout weapon's reserve (an ammo pickup such as the armory's).
    // pawn:    the pawn.
    // weapons: the loadout's stats.
    void refillLoadout(Pawn& pawn, const WeaponStatsTable& weapons);

    // Host: puts the weapon in hand away and takes another: a running reload is dropped, the
    // rounds swap with the stowed ones (the reload counter carries over, so no old press replays)
    // and the switch time starts (holster plus draw).
    // pawn:    the pawn.
    // weapons: the loadout's stats.
    // weapon:  the weapon to take.
    void switchWeapon(Pawn& pawn, const WeaponStatsTable& weapons, WeaponKind weapon);
} //namespace game

#endif //LOCKSTEP_GAME_WEAPONS_LOADOUT_HPP
