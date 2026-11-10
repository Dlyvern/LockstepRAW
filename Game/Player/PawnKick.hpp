#ifndef LOCKSTEP_GAME_PLAYER_PAWN_KICK_HPP
#define LOCKSTEP_GAME_PLAYER_PAWN_KICK_HPP

#include "Player/PawnMovement.hpp"
#include "Player/PlayerCommand.hpp"

namespace game
{
    // Advances a pawn's kick timers for one tick: a fresh kick press on the ground (not sliding,
    // mantling or vaulting, cooldown over) starts a kick; a running kick counts its ticks and ends
    // after KickSettings::seconds, starting the cooldown. The weapon can't fire while kicking.
    // Part of stepPawn(), so the client predicts it.
    // motion:   the pawn's motion, updated.
    // command:  the command of this tick.
    // settings: movement settings (PawnSettings::kick).
    // delta:    the Player domain's delta for this tick, in seconds.
    void updateKick(PawnMotion& motion, const PlayerCommand& command, const PawnSettings& settings, float delta);

    // motion:  a pawn's motion.
    // Returns: true while it kicks.
    [[nodiscard]] bool isKicking(const PawnMotion& motion);

    // motion:   a pawn's motion after this tick's step.
    // settings: movement settings.
    // delta:    the Player domain's delta for one tick.
    // Returns:  true on the one tick the kick strikes (the host resolves its hits then).
    [[nodiscard]] bool isKickImpact(const PawnMotion& motion, const PawnSettings& settings, float delta);

    // motion:   a pawn's motion.
    // delta:    the Player domain's delta for one tick.
    // Returns:  seconds since the kick started, or a negative value while not kicking.
    [[nodiscard]] float kickSeconds(const PawnMotion& motion, float delta);

    // motion:   a pawn's motion.
    // settings: movement settings.
    // Returns:  the speed multiplier of its movement (KickSettings::moveScale while kicking, else 1).
    [[nodiscard]] float kickSpeedScale(const PawnMotion& motion, const PawnSettings& settings);
} //namespace game

#endif //LOCKSTEP_GAME_PLAYER_PAWN_KICK_HPP
