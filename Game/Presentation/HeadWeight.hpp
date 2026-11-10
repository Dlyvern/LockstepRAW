#ifndef LOCKSTEP_GAME_PRESENTATION_HEAD_WEIGHT_HPP
#define LOCKSTEP_GAME_PRESENTATION_HEAD_WEIGHT_HPP

#include <Lockstep/Core/Math.hpp>

#include <cstdint>

namespace game
{
    namespace math = lockstep::math;

    // Feel of the body's weight in the first-person camera, editable from the dev panel. Impulses
    // feed damped springs, so every reaction overshoots a little and settles.
    // accelPitch:    pitch rate (rad/s) per m/s of forward velocity change; positive leans the
    //                view down when speeding up and back when stopping.
    // accelRoll:     roll rate (rad/s) per m/s of sideways velocity change (leans into a strafe
    //                start or a direction change).
    // leanStiffness, leanDamping: the spring of those leans.
    // leanLimit:     largest lean pitch or roll, radians.
    // turnRoll:      roll per m/s of speed times rad/s of turn rate (leans into a turn while
    //                moving), radians.
    // turnRollLimit: largest turn roll, radians.
    // headLag:       how far the view trails a fast turn, radians per rad/s of turn rate.
    // headLagLimit:  largest trail, radians.
    // aimLagScale:   head lag and turn roll multiplier while aiming down sights.
    // turnSmoothRate: how fast the measured turn rate follows the look, 1/s.
    // plantDrop:     downward eye speed (m/s) a footfall adds at walking bob strength.
    // plantPitch:    downward pitch rate (rad/s) a footfall adds at walking bob strength.
    // plantStiffness, plantDamping: the spring of the footfall plant.
    // jumpPitch:     pitch rate (rad/s) the take-off of a jump adds (down).
    // landPitch:     pitch rate (rad/s) per m/s of fall speed a landing adds (down).
    // landPitchMinSpeed: falls slower than this (m/s) don't nod.
    // vaultPitch:    pitch rate (rad/s) a vault adds as the hand plants (down).
    // kickPitch:     pitch rate (rad/s) a kick adds as it starts (down, towards the leg).
    // kickImpactPitch: pitch rate (rad/s) the kick's strike adds (up, the jolt back).
    // kickPush:      forward eye speed (m/s) the strike adds (the body follows the leg).
    // pushStiffness, pushDamping: the spring of that push.
    struct HeadWeightTuning
    {
        float accelPitch{0.06f};
        float accelRoll{0.06f};
        float leanStiffness{150.0f};
        float leanDamping{15.0f};
        float leanLimit{0.045f};
        float turnRoll{0.0035f};
        float turnRollLimit{0.03f};
        float headLag{0.0025f};
        float headLagLimit{0.01f};
        float aimLagScale{0.0f};
        float turnSmoothRate{20.0f};
        float plantDrop{0.10f};
        float plantPitch{0.05f};
        float plantStiffness{420.0f};
        float plantDamping{28.0f};
        float jumpPitch{0.25f};
        float landPitch{0.07f};
        float landPitchMinSpeed{2.5f};
        float vaultPitch{0.45f};
        float kickPitch{1.1f};
        float kickImpactPitch{0.45f};
        float kickPush{0.9f};
        float pushStiffness{160.0f};
        float pushDamping{16.0f};
    };

    // State of the weight reactions. Presentation only.
    // lean, leanRate:   (pitch, roll) lean in radians and its rate.
    // plant, plantRate: (drop in metres, pitch in radians) of the footfall plant and its rate.
    // push, pushRate:   forward eye offset of a kick, metres, and its rate.
    // turnRate:         smoothed look turn rate, rad/s (positive turns left).
    // yawLag:           how far the view trails the look, radians.
    // lastVelocity:     planar velocity of the previous frame, m/s.
    // lastYaw:          look yaw of the previous frame.
    // hasLast:          lastVelocity and lastYaw are valid.
    struct HeadWeightState
    {
        math::Vec2 lean{0.0f, 0.0f};
        math::Vec2 leanRate{0.0f, 0.0f};
        math::Vec2 plant{0.0f, 0.0f};
        math::Vec2 plantRate{0.0f, 0.0f};
        float push{0.0f};
        float pushRate{0.0f};
        float turnRate{0.0f};
        float yawLag{0.0f};
        math::Vec3 lastVelocity{0.0f, 0.0f, 0.0f};
        float lastYaw{0.0f};
        bool hasLast{false};
    };

    // What drives the weight reactions this frame.
    // yaw:            look yaw shown this frame.
    // planarVelocity: the pawn's horizontal velocity, m/s.
    // isOnFeet:       on the ground and not sliding (leans and turn roll apply).
    // isAiming:       aiming down sights (head lag and turn roll scaled by aimLagScale).
    // footfalls:      footfalls this frame (from the head bob, the same that play footstep sounds).
    // stepStrength:   bob size relative to walking (HeadPose::bobStrength).
    // isTakeoff:      a jump left the ground this frame.
    // isVaultStart:   a vault started this frame.
    // isKickStart:    a kick started this frame.
    // isKickImpact:   the kick struck this frame.
    // landSpeed:      fall speed of a landing this frame, m/s; 0 without one.
    // seconds:        Player time since the previous frame.
    struct HeadWeightInput
    {
        float yaw{0.0f};
        math::Vec3 planarVelocity{0.0f, 0.0f, 0.0f};
        bool isOnFeet{true};
        bool isAiming{false};
        uint32_t footfalls{0};
        float stepStrength{0.0f};
        bool isTakeoff{false};
        bool isVaultStart{false};
        bool isKickStart{false};
        bool isKickImpact{false};
        float landSpeed{0.0f};
        float seconds{0.0f};
    };

    // What the weight reactions add to the camera.
    // pitch:     radians added to the view pitch (negative looks down).
    // roll:      radians added to the camera roll (positive tilts the right ear down).
    // yawOffset: radians added to the view yaw (the head trailing a turn).
    // drop:      metres added to the eye height (negative is down).
    // forward:   metres the eye moves along the look (a kick's push).
    struct HeadWeightPose
    {
        float pitch{0.0f};
        float roll{0.0f};
        float yawOffset{0.0f};
        float drop{0.0f};
        float forward{0.0f};
    };

    // Advances the weight reactions by one frame: leans into speed and direction changes, rolls
    // into turns while moving, trails fast turns slightly, plants each footfall, nods on take-off
    // and landing.
    // state:   the reactions, updated.
    // tuning:  their feel.
    // input:   this frame's movement and events.
    // Returns: what to add to the camera.
    [[nodiscard]] HeadWeightPose updateHeadWeight(HeadWeightState& state, const HeadWeightTuning& tuning,
        const HeadWeightInput& input);

#if LOCKSTEP_DEV_TOOLS
    // Draws the weight tuning controls of the Game dev panel.
    // state:  the reactions (shown).
    // tuning: their feel, edited.
    void headWeightDevSection(const HeadWeightState& state, HeadWeightTuning& tuning);
#endif
} //namespace game

#endif //LOCKSTEP_GAME_PRESENTATION_HEAD_WEIGHT_HPP
