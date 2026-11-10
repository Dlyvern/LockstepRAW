#ifndef LOCKSTEP_GAME_PLAYER_WEAPON_DRILL_HPP
#define LOCKSTEP_GAME_PLAYER_WEAPON_DRILL_HPP

#if LOCKSTEP_DEV_TOOLS

#include "Player/PlayerCommand.hpp"

#include <cstdint>
#include <span>

namespace game
{
    // Dev: picks the local weapon from the command line, for captures and host/client checks
    // without a person. `--dev-weapon <m4|glock|kriss>` holds that weapon; `--dev-switch-drill
    // <seconds>` switches to the next weapon (M4, Glock, KRISS, ...) every that many seconds, with
    // the normal holster and draw. Combine with `--dev-fire-drill` to fire and reload each one.
    // controls:  the local controls, changed in place.
    // weapons:   the loadout's stats (switch times).
    // arguments: the command line.
    // tick:      the simulation tick.
    // delta:     seconds per tick.
    void applyWeaponDrill(LocalControls& controls, const WeaponStatsTable& weapons,
        std::span<const char* const> arguments, uint64_t tick, float delta);
} //namespace game

#endif

#endif //LOCKSTEP_GAME_PLAYER_WEAPON_DRILL_HPP
