#ifndef LOCKSTEP_GAME_PLAYER_KICK_SETTINGS_HPP
#define LOCKSTEP_GAME_PLAYER_KICK_SETTINGS_HPP

#include <cstdint>

namespace game
{
    // The front kick (V): its timing, part of the predicted movement, and what it does, decided
    // by the host. Host and client must use the same timing values.
    // seconds:       length of a kick, from the press until the leg is back.
    // impactSeconds: time from the press to the moment it strikes.
    // cooldownSeconds: Player seconds after a kick ends before the next may start.
    // moveScale:     speed multiplier while kicking (the pawn plants its standing leg).
    // range:         how far in front of the pawn the kick reaches, metres (from the capsule's axis).
    // coneCos:       cosine of the half angle of the cone it hits in (0.8 is about 37 degrees).
    // height:        height above the feet the kick strikes at, metres.
    // damage:        hit points a kicked merc loses.
    // shove:         how far a kicked living NPC is pushed back, metres (swept, so it stops at walls).
    // bodySpeed:     velocity change of a kicked prop or toy, m/s, plus bodyLift upwards.
    // bodyLift:      see bodySpeed.
    struct KickSettings
    {
        float seconds{0.5f};
        float impactSeconds{0.17f};
        float cooldownSeconds{0.2f};
        float moveScale{0.35f};
        float range{1.2f};
        float coneCos{0.8f};
        float height{0.85f};
        int32_t damage{30};
        float shove{0.7f};
        float bodySpeed{7.0f};
        float bodyLift{2.0f};
    };
} //namespace game

#endif //LOCKSTEP_GAME_PLAYER_KICK_SETTINGS_HPP
