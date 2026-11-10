#ifndef LOCKSTEP_GAME_PLAYER_MOVE_DRILL_HPP
#define LOCKSTEP_GAME_PLAYER_MOVE_DRILL_HPP

#if LOCKSTEP_DEV_TOOLS

#include "Player/PlayerCommand.hpp"
#include "Player/Prediction.hpp"

#include <cstdint>
#include <span>

namespace game
{
    // Dev (`--dev-move-drill`): replaces the local player's movement input with a scripted 12 s
    // drill (walk, sprint, slide, stop, strafe both ways, back, sprint jump, turning run, idle,
    // facing the other way every other round), so prediction can be checked without a person.
    // arguments: the command line.
    // Returns:   true when the drill is on.
    [[nodiscard]] bool isMoveDrillRequested(std::span<const char* const> arguments);

    // Overwrites the movement part of a command (move, sprint, crouch, jump, yaw) with the
    // drill's input for a tick.
    // command: the command sampled from local input, changed in place.
    // tick:    the simulation tick.
    // delta:   seconds per tick.
    void applyMoveDrill(PlayerCommand& command, uint64_t tick, float delta);

    // Logs the client's prediction counters once per drill round.
    // prediction: the client's prediction.
    // tick:       the simulation tick.
    // delta:      seconds per tick.
    void logMoveDrill(const Prediction& prediction, uint64_t tick, float delta);
} //namespace game

#endif

#endif //LOCKSTEP_GAME_PLAYER_MOVE_DRILL_HPP
