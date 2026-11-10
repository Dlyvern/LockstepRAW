#ifndef LOCKSTEP_GAME_CHARACTERS_WEAPON_SWAY_HPP
#define LOCKSTEP_GAME_CHARACTERS_WEAPON_SWAY_HPP

#include <Lockstep/Core/Math.hpp>

#include <cstdint>

namespace game
{
    namespace math = lockstep::math;

    // The weapon kick: how each shot throws the drawn weapon (the camera recoil that moves the aim
    // is RecoilSettings). The sights move with it, so the next rounds follow. Editable from the
    // dev panel.
    // climb:          muzzle climb speed one shot adds, rad/s.
    // yaw:            largest sideways muzzle speed one shot adds (random side), rad/s.
    // roll:           roll speed one shot adds (mostly clockwise), rad/s.
    // back:           backward speed one shot adds, m/s.
    // firstShotScale: multiplier of the first shot of a string.
    // build:          added per shot of sustained fire.
    // maxBuild:       largest sustained-fire multiplier.
    // streakSeconds:  a pause this long ends a string of fire.
    // stiffness, damping: the muzzle climb and sideways springs.
    // rollStiffness, rollDamping: the roll spring.
    // backStiffness, backDamping: the back kick spring.
    // aimScale:       multiplier while aiming down sights.
    // crouchScale:    multiplier while crouched or sliding.
    // maxAngle:       largest kick angle, radians.
    // maxBack:        largest back kick, metres.
    struct WeaponKickTuning
    {
        float climb{0.75f};
        float yaw{0.3f};
        float roll{1.4f};
        float back{0.6f};
        float firstShotScale{1.5f};
        float build{0.04f};
        float maxBuild{1.5f};
        float streakSeconds{0.25f};
        float stiffness{300.0f};
        float damping{24.0f};
        float rollStiffness{220.0f};
        float rollDamping{16.0f};
        float backStiffness{450.0f};
        float backDamping{32.0f};
        float aimScale{0.45f};
        float crouchScale{0.7f};
        float maxAngle{0.15f};
        float maxBack{0.05f};
    };

    // The weapon kick's springs.
    // angle, rate: (pitch, yaw, roll) of the kick in radians, and its rate.
    // back, backRate: backward offset in metres, and its rate.
    // streak:      shots in the current string of fire.
    // sinceShot:   seconds since the last shot.
    // random:      presentation-only random state (sideways side, roll variation).
    struct WeaponKickState
    {
        math::Vec3 angle{0.0f, 0.0f, 0.0f};
        math::Vec3 rate{0.0f, 0.0f, 0.0f};
        float back{0.0f};
        float backRate{0.0f};
        uint32_t streak{0};
        float sinceShot{1.0f};
        uint32_t random{0x1B873593u};
    };

    // Breathing sway and the held breath, editable from the dev panel. Stamina drains while
    // running and recovers otherwise; low stamina makes breathing faster and the sway larger.
    // Holding the breath (Shift while aiming) steadies the weapon for a while, then it shakes
    // until the breath is let go; after that the breath must recover before the next hold.
    // hipAmount:      breathing sway from the hip, radians.
    // aimAmount:      breathing sway aiming down sights, radians.
    // rate:           breaths per second at rest.
    // exhaustedRate:  breaths per second with no stamina.
    // exhaustedScale: sway multiplier with no stamina.
    // sprintDrain:    stamina lost per second of running (stamina runs 0 to 1).
    // recoverRate:    stamina regained per second otherwise.
    // holdSeconds:    how long a held breath steadies the weapon.
    // holdScale:      sway multiplier while the breath is held.
    // holdBlendRate:  how fast the hold steadies and lets go, 1/s.
    // shakeScale:     sway multiplier once the held breath runs out.
    // tremble:        extra fast tremble once it runs out, radians.
    // recoverSeconds: how long a full hold needs to recover (shorter holds recover sooner).
    // holdStamina:    stamina a full hold costs.
    struct BreathTuning
    {
        float hipAmount{0.004f};
        float aimAmount{0.0028f};
        float rate{0.25f};
        float exhaustedRate{0.6f};
        float exhaustedScale{3.5f};
        float sprintDrain{0.14f};
        float recoverRate{0.1f};
        float holdSeconds{3.0f};
        float holdScale{0.12f};
        float holdBlendRate{6.0f};
        float shakeScale{2.0f};
        float tremble{0.0018f};
        float recoverSeconds{2.5f};
        float holdStamina{0.2f};
    };

    // The local breathing.
    // stamina:      0 (out of breath) to 1.
    // phase:        breathing cycle, radians.
    // isHolding:    the breath is held.
    // heldSeconds:  how long the current hold has lasted.
    // recovery:     seconds until the breath can be held again.
    // steadiness:   0 to 1, how far the hold has steadied the weapon (eased).
    // shake:        0 to 1, how far the spent hold shakes it (eased).
    // tremblePhase: phase of the tremble, radians.
    // holds, releases: holds started and let go so far (wrapping), for sounds.
    struct BreathState
    {
        float stamina{1.0f};
        float phase{0.0f};
        bool isHolding{false};
        float heldSeconds{0.0f};
        float recovery{0.0f};
        float steadiness{0.0f};
        float shake{0.0f};
        float tremblePhase{0.0f};
        uint32_t holds{0};
        uint32_t releases{0};
    };

    // What drives the breathing this frame.
    // aimed:       0 hip to 1 aiming down sights (eased).
    // isSprinting: the player runs.
    // wantsHold:   the hold-breath key is down while aiming.
    // seconds:     Player time since the previous frame.
    struct BreathInput
    {
        float aimed{0.0f};
        bool isSprinting{false};
        bool wantsHold{false};
        float seconds{0.0f};
    };

    // Adds one shot's kick.
    // kick:       the kick, updated.
    // tuning:     its feel.
    // isAiming:   aiming down sights.
    // isCrouched: crouched or sliding.
    void kickWeapon(WeaponKickState& kick, const WeaponKickTuning& tuning, bool isAiming, bool isCrouched);

    // Springs the kick back.
    // kick:    the kick, updated.
    // tuning:  its feel.
    // seconds: Player time since the previous frame.
    void stepWeaponKick(WeaponKickState& kick, const WeaponKickTuning& tuning, float seconds);

    // Advances the breathing.
    // breath:  the breathing, updated.
    // tuning:  its feel.
    // input:   this frame's aim, running and hold key.
    // Returns: the breathing sway as (yaw, pitch) radians.
    [[nodiscard]] math::Vec2 stepBreath(BreathState& breath, const BreathTuning& tuning, const BreathInput& input);

#if LOCKSTEP_DEV_TOOLS
    // Draws the weapon kick and breathing controls of the Game dev panel.
    // kick:          the kick feel, edited.
    // breath:        the breathing feel, edited.
    // breathState:   the current breathing, shown.
    void weaponSwayDevSection(WeaponKickTuning& kick, BreathTuning& breath, const BreathState& breathState);
#endif
} //namespace game

#endif //LOCKSTEP_GAME_CHARACTERS_WEAPON_SWAY_HPP
