#ifndef LOCKSTEP_GAME_PLAYER_FIRE_DRILL_HPP
#define LOCKSTEP_GAME_PLAYER_FIRE_DRILL_HPP

#if LOCKSTEP_DEV_TOOLS

#include "Player/PlayerCommand.hpp"

#include <cstdint>
#include <optional>
#include <span>

namespace game
{
    // Dev (`--dev-fire-drill`): scripts the local player's trigger in an 8 s round (three semi
    // clicks from the hip, a second of auto aiming down sights, a burst aiming, a second of auto
    // from the hip, then a pause), turning a little each round and with the flashlight on, so
    // fire modes, recoil, the host's cadence check, penetration and the flashlight replication
    // can be checked without a person.
    // arguments: the command line.
    // Returns:   true when the drill is on.
    [[nodiscard]] bool isFireDrillRequested(std::span<const char* const> arguments);

    // Sets up one tick of the drill: the fire mode, the flashlight and the look's turn.
    // controls: the local controls, changed in place.
    // tick:     the simulation tick.
    // delta:    seconds per tick.
    // Returns:  the fire and aim buttons of this tick.
    [[nodiscard]] FireButtons fireDrillButtons(LocalControls& controls, uint64_t tick, float delta);

    // Dev (`--dev-scope-drill`): scripts aiming through the optic in a 10 s round (a second from
    // the hip, then eight seconds aimed: still, a slow sideways sweep, three semi shots at 5 s and
    // a short auto burst at 7 s), turning 0.9 rad to the right each round with the flashlight off, so the
    // scope view, its reticle and shadow under sway and recoil, and its GPU cost can be checked
    // without a person (`--dev-iron-sights` compares against iron sights).
    // arguments: the command line.
    // Returns:   true when the drill is on.
    [[nodiscard]] bool isScopeDrillRequested(std::span<const char* const> arguments);

    // Sets up one tick of the scope drill: fire mode, flashlight and the look's turn and sweep.
    // controls: the local controls, changed in place.
    // tick:     the simulation tick.
    // delta:    seconds per tick.
    // Returns:  the fire and aim buttons of this tick.
    [[nodiscard]] FireButtons scopeDrillButtons(LocalControls& controls, uint64_t tick, float delta);
} //namespace game

#endif

#endif //LOCKSTEP_GAME_PLAYER_FIRE_DRILL_HPP
