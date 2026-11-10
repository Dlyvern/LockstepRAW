#ifndef LOCKSTEP_GAME_PLAYER_MOVE_DRILL_HPP
#define LOCKSTEP_GAME_PLAYER_MOVE_DRILL_HPP

#if LOCKSTEP_DEV_TOOLS

#include "Player/PlayerCommand.hpp"
#include "Player/Prediction.hpp"

#include <cstdint>
#include <optional>
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

    // Dev (`--dev-vault-drill`): steers the local player back and forth across the concrete road
    // barrier row of hospital_a (z = 12.05, from x = 36 on), pressing jump in front of it, in 6 s
    // rounds that alternate walking, sprinting and sliding into it and use different barriers.
    // arguments: the command line.
    // Returns:   true when the drill is on.
    [[nodiscard]] bool isVaultDrillRequested(std::span<const char* const> arguments);

    // Overwrites the movement part of a command with the vault drill's input for a tick.
    // command: the command sampled from local input, changed in place.
    // foot:    the local pawn's foot position.
    // tick:    the simulation tick.
    // delta:   seconds per tick.
    void applyVaultDrill(PlayerCommand& command, const math::Vec3& foot, uint64_t tick, float delta);

    // Dev (`--dev-kick-drill`): presses kick every 1.2 s and sweeps the look from a little down to
    // nearly straight down every 7 s (the legs in first person); with a target
    // (e.g. the nearest NPC) it faces it and walks up to it first. So the kick can be watched and
    // captured without a person; combines with the other drills.
    // arguments: the command line.
    // Returns:   true when the drill is on.
    [[nodiscard]] bool isKickDrillRequested(std::span<const char* const> arguments);

    // Overwrites the kick and look of a command with the kick drill's for a tick.
    // command: the command sampled from local input, changed in place.
    // tick:    the simulation tick.
    // delta:   seconds per tick.
    // foot:    the local pawn's foot position.
    // target:  a point to face (yaw only), or nothing to keep the look's yaw.
    void applyKickDrill(PlayerCommand& command, uint64_t tick, float delta, const math::Vec3& foot,
        const std::optional<math::Vec3>& target);

    // Dev (`--dev-watch-npc`): turns the look to the nearest living NPC every tick (to capture how
    // NPCs move and crouch); combines with the other drills.
    // arguments: the command line.
    // Returns:   true when it is on.
    [[nodiscard]] bool isWatchNpcRequested(std::span<const char* const> arguments);

    // Turns a command's look at a point.
    // command: the command, changed in place.
    // eye:     the local pawn's eye position.
    // target:  the point to look at.
    void lookAtPoint(PlayerCommand& command, const math::Vec3& eye, const math::Vec3& target);

    // Logs the client's prediction counters once per drill round.
    // prediction: the client's prediction.
    // tick:       the simulation tick.
    // delta:      seconds per tick.
    void logMoveDrill(const Prediction& prediction, uint64_t tick, float delta);
} //namespace game

#endif

#endif //LOCKSTEP_GAME_PLAYER_MOVE_DRILL_HPP
