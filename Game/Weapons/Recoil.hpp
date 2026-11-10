#ifndef LOCKSTEP_GAME_WEAPONS_RECOIL_HPP
#define LOCKSTEP_GAME_WEAPONS_RECOIL_HPP

#include <Lockstep/Core/Math.hpp>

#include <cstdint>

namespace game
{
    namespace math = lockstep::math;

    // The camera recoil: the part of the kick that moves the aim itself, so it is in the command's
    // look and the host traces along it. A learnable pattern: every shot climbs, sustained fire
    // climbs harder, the first shot of a string kicks most; the horizontal part wanders randomly
    // within a band. When firing stops the aim settles back down. Editable from the dev panel.
    // climb:          pitch climb of one shot, radians.
    // firstShotScale: climb multiplier of the first shot of a string.
    // build:          climb added per shot of sustained fire, relative to `climb`.
    // maxBuild:       largest climb multiplier sustained fire reaches.
    // wander:         largest horizontal step of one shot, radians.
    // wanderLimit:    horizontal band the wander stays in, radians.
    // wanderPull:     fraction of the horizontal offset each shot pulls back towards the centre.
    // kickSeconds:    time a shot's kick takes to reach the aim (it is applied over a few ticks).
    // recoverDelay:   seconds without a shot before the aim settles back.
    // recoverRate:    how fast it settles, 1/s.
    // recoverAmount:  fraction of the climb that settles back (the rest stays in the look).
    // streakSeconds:  a pause this long ends a string of fire.
    // aimScale:       multiplier while aiming down sights.
    // crouchScale:    multiplier while crouched.
    struct RecoilSettings
    {
        float climb{0.0065f};
        float firstShotScale{1.7f};
        float build{0.05f};
        float maxBuild{1.7f};
        float wander{0.0045f};
        float wanderLimit{0.028f};
        float wanderPull{0.15f};
        float kickSeconds{0.045f};
        float recoverDelay{0.14f};
        float recoverRate{6.0f};
        float recoverAmount{0.9f};
        float streakSeconds{0.25f};
        float aimScale{0.7f};
        float crouchScale{0.75f};
    };

    // The local camera recoil, kept between ticks with the local controls.
    // offset:     (yaw, pitch) the recoil currently adds to the look, radians.
    // pending:    kick of recent shots still to be applied.
    // streak:     shots in the current string of fire.
    // sinceShot:  Player seconds since the last shot.
    // random:     state of the horizontal wander.
    struct RecoilState
    {
        math::Vec2 offset{0.0f, 0.0f};
        math::Vec2 pending{0.0f, 0.0f};
        uint32_t streak{0};
        float sinceShot{1.0f};
        uint32_t random{0x9E3779B9u};
    };

    // Adds one shot's kick.
    // recoil:     the recoil, updated.
    // settings:   the pattern.
    // isAiming:   the shot was fired aiming down sights.
    // isCrouched: the shooter is crouched.
    void kickRecoil(RecoilState& recoil, const RecoilSettings& settings, bool isAiming, bool isCrouched);

    // Advances the recoil by one tick: moves the pending kick into the offset and, after a pause
    // in fire, settles the offset back.
    // recoil:       the recoil, updated.
    // settings:     the pattern.
    // deltaSeconds: the Player domain's delta of this tick.
    // Returns:      (yaw, pitch) of the settled offset that does not recover; the caller adds it
    //               to the base look so it stays.
    [[nodiscard]] math::Vec2 stepRecoil(RecoilState& recoil, const RecoilSettings& settings, float deltaSeconds);
} //namespace game

#endif //LOCKSTEP_GAME_WEAPONS_RECOIL_HPP
