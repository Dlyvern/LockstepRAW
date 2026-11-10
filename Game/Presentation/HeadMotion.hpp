#ifndef LOCKSTEP_GAME_PRESENTATION_HEAD_MOTION_HPP
#define LOCKSTEP_GAME_PRESENTATION_HEAD_MOTION_HPP

#include "Presentation/HeadWeight.hpp"

#include <Lockstep/Core/Math.hpp>

#include <cstdint>

namespace game
{
    namespace math = lockstep::math;

    // Feel of the first-person camera's procedural motion, editable from the dev panel.
    // eyeHeightRate: how fast the eye follows a stance change (crouch, slide, stand), 1/s.
    // stepSmoothRate: how fast the eye catches up with a step the controller climbed or dropped
    //               (stairs), 1/s; 25 settles in about 0.1 s.
    // stepMinSpeed:  vertical foot speed (m/s) on the ground from which a height change counts as a
    //               step rather than a ramp.
    // stepMaxOffset: largest step smoothed, metres; bigger jumps (teleports) snap.
    // walkStride, runStride, crouchStride: metres travelled per bob cycle (two steps); set from
    //               the character clips when they load, so the bob keeps the drawn legs' cadence.
    // walkStrideSpeed, runStrideSpeed: speeds (m/s) at which the walk and the run stride apply;
    //               in between they blend (the clips' own speeds, as the character blends them).
    // walkBob, runBob, crouchBob: vertical bob depth in metres at that gait. The bob bottoms out
    //               sharply at each footfall (the moment the footstep sound plays).
    // bobSide:       sideways bob as a fraction of the vertical one.
    // bobSmoothing:  how fast the bob fades in and out, 1/s.
    // landImpulse:   downward eye speed per m/s of fall speed on landing (the compression grows
    //               with the fall).
    // landMinSpeed:  falls slower than this (m/s) don't dip.
    // landMinAirSeconds: shorter times off the ground (running down stairs) don't count as a
    //               landing.
    // landMaxDip:    deepest landing dip, metres.
    // landStiffness, landDamping: the landing spring.
    // jumpDip:       downward eye speed (m/s) when a jump leaves the ground (the take-off push).
    // strafeRoll:    roll in radians at full strafe speed (leans into the strafe).
    // slideRoll:     roll while sliding, radians (negative tilts left).
    // vaultRoll:     roll while vaulting, radians (negative tilts left, towards the planted hand).
    // vaultSmoothRate: how fast the eye catches up with the vault's rise, 1/s (lower rounds the
    //               hop into an arc).
    // vaultMaxOffset: largest vault rise smoothed, metres.
    // rollRate:      how fast roll follows its target, 1/s.
    // recoilKick:    camera pitch velocity one shot adds, rad/s: a quick punch on top of the camera
    //                recoil in the command's look (aiming down sights the shots follow it, since
    //                they follow the sights).
    // recoilStiffness, recoilDamping: the camera recoil spring.
    // aimRecoilScale: camera recoil multiplier while aiming down sights.
    // crouchRecoilScale: camera recoil multiplier while crouched or sliding.
    // leanRoll:      camera roll at a full lean, radians.
    // sprintFov:     field of view added while running, radians.
    // bulletTimeFov: field of view added in bullet-time, radians (negative narrows it).
    // sprintFovRate: how fast the field of view widens into a run, 1/s.
    // fovRate:       how fast it narrows back (and follows bullet-time), 1/s.
    // bulletTimeRate: how fast the bullet-time heaviness comes and goes, 1/s.
    // bulletTimeBob: bob depth multiplier in bullet-time.
    // bulletTimeRoll: roll rate multiplier in bullet-time (the head turns heavier).
    // bulletTimeLandDamping: landing spring damping multiplier in bullet-time.
    // weight:        the body's weight reactions (leans, footfall plant, turn roll, head lag).
    // fieldOfViewOffset: the player's field of view option minus the default, radians; added to
    //               every pose's fovOffset.
    struct HeadMotionTuning
    {
        float eyeHeightRate{12.0f};
        float stepSmoothRate{25.0f};
        float stepMinSpeed{5.0f};
        float stepMaxOffset{0.45f};
        float walkStride{1.7f};
        float runStride{2.75f};
        float crouchStride{1.25f};
        float walkStrideSpeed{1.6f};
        float runStrideSpeed{3.75f};
        float walkBob{0.022f};
        float runBob{0.04f};
        float crouchBob{0.012f};
        float bobSide{0.45f};
        float bobSmoothing{8.0f};
        float landImpulse{0.42f};
        float landMinSpeed{1.5f};
        float landMinAirSeconds{0.12f};
        float landMaxDip{0.22f};
        float landStiffness{140.0f};
        float landDamping{13.0f};
        float jumpDip{0.55f};
        float strafeRoll{0.018f};
        float slideRoll{-0.07f};
        float vaultRoll{-0.08f};
        float vaultSmoothRate{13.0f};
        float vaultMaxOffset{1.6f};
        float rollRate{8.0f};
        float recoilKick{0.35f};
        float recoilStiffness{160.0f};
        float recoilDamping{20.0f};
        float aimRecoilScale{0.5f};
        float crouchRecoilScale{0.75f};
        float leanRoll{0.17f};
        float sprintFov{0.085f};
        float bulletTimeFov{-0.05f};
        float sprintFovRate{11.0f};
        float fovRate{6.0f};
        float bulletTimeRate{4.0f};
        float bulletTimeBob{0.75f};
        float bulletTimeRoll{0.5f};
        float bulletTimeLandDamping{1.4f};
        HeadWeightTuning weight{};
        float fieldOfViewOffset{0.0f};
    };

    // What drives the head motion this frame, from the local pawn's predicted (client) or
    // simulated (host) state.
    // foot:          the pawn's drawn foot position.
    // eyeHeight:     the upright eye height above the feet of its current stance.
    // leanOffset:    where the lean moves the eyes from there (sideways and slightly down).
    // lean:          the lean, -1 (full left) to 1 (full right).
    // yaw:           look angle (as in PlayerCommand).
    // planarVelocity: horizontal velocity, m/s.
    // verticalSpeed: upward speed, m/s.
    // isGrounded:    on the ground.
    // isCrouched:    crouched or sliding.
    // isSliding:     sliding.
    // isVaulting:    vaulting over an obstacle.
    // kickSeconds:   seconds into the pawn's kick, negative while not kicking.
    // kickImpactSeconds: when in the kick it strikes (KickSettings::impactSeconds).
    // isAiming:      aiming down sights.
    // isSprinting:   running (field of view kick).
    // isBulletTime:  the World domain is slowed (the head feels heavier, the view narrows; the
    //                pawn's own speed is unchanged).
    // walkSpeed, runSpeed, strafeSpeed: movement speeds the bob and roll are scaled by (the
    //               PawnSettings values).
    // shotCount:     the local shot counter; each increase kicks when `canKick`.
    // canKick:       a shot fires the weapon (armed, weapon up, no toy held).
    // deltaSeconds:  Player time since the previous frame.
    struct HeadMotionInput
    {
        math::Vec3 foot{0.0f, 0.0f, 0.0f};
        float eyeHeight{1.6f};
        math::Vec3 leanOffset{0.0f, 0.0f, 0.0f};
        float lean{0.0f};
        float yaw{0.0f};
        math::Vec3 planarVelocity{0.0f, 0.0f, 0.0f};
        float verticalSpeed{0.0f};
        bool isGrounded{true};
        bool isCrouched{false};
        bool isSliding{false};
        bool isVaulting{false};
        float kickSeconds{-1.0f};
        float kickImpactSeconds{0.17f};
        bool isAiming{false};
        bool isSprinting{false};
        bool isBulletTime{false};
        float walkSpeed{3.8f};
        float runSpeed{6.4f};
        float strafeSpeed{3.4f};
        uint8_t shotCount{0};
        bool canKick{false};
        float deltaSeconds{0.0f};
    };

    // Where the camera sits this frame. Presentation only: the command's yaw and pitch (the aim the
    // host checks) never include any of it.
    // eye:         camera position (smoothed eye height, bob and landing dip).
    // pitchOffset: recoil kick and body weight added to the look pitch, radians.
    // yawOffset:   head lag added to the look yaw, radians.
    // roll:        camera roll, radians (positive tilts the right ear down).
    // bobPhase:    the step cycle, radians, for the viewmodel's bob.
    // bobStrength: bob size relative to walking (0 still, about 2 running), for the viewmodel.
    // footfalls:   steps taken so far (one per half bob cycle, at the bottom of the bob), wrapping;
    //              footstep sounds play on each.
    // landings:    landings so far, wrapping.
    // landSpeed:   fall speed of the newest landing, m/s.
    // fovOffset:   field of view to add (running, bullet-time), radians.
    struct HeadPose
    {
        math::Vec3 eye{0.0f, 0.0f, 0.0f};
        float pitchOffset{0.0f};
        float yawOffset{0.0f};
        float roll{0.0f};
        float bobPhase{0.0f};
        float bobStrength{0.0f};
        uint32_t footfalls{0};
        uint32_t landings{0};
        float landSpeed{0.0f};
        float fovOffset{0.0f};
    };

    // The local camera's procedural motion. Presentation only, never replicated.
    // tuning:       feel settings.
    // isActive:     updated last frame; false snaps the eye height on the next update.
    // eyeHeight:    smoothed eye height above the feet.
    // bobPhase:     step cycle, radians.
    // bobWeight:    smoothed bob strength (0 still, 1 moving).
    // bobDepth:     smoothed bob depth of the current gait, metres.
    // dip, dipRate: landing dip (negative is down) and its rate.
    // wasGrounded:  grounded last frame.
    // fallSpeed:    fastest fall since leaving the ground, m/s.
    // airSeconds:   time since leaving the ground.
    // footfalls, landings, landSpeed: as in HeadPose.
    // roll:         smoothed camera roll.
    // kick, kickRate: camera recoil pitch and its rate.
    // fovOffset:    smoothed field of view offset, radians.
    // bulletTimeWeight: 0 normal to 1 fully in bullet-time, eased.
    // shotCount:    the local shot counter last seen.
    // weight:       the body's weight reactions.
    // wasVaulting:  vaulting last frame.
    // lastKickSeconds: HeadMotionInput::kickSeconds of the last frame.
    struct HeadMotionState
    {
        HeadMotionTuning tuning{};
        bool isActive{false};
        float eyeHeight{1.6f};
        float stepOffset{0.0f};
        float lastFootY{0.0f};
        float bobPhase{0.0f};
        float bobWeight{0.0f};
        float bobDepth{0.0f};
        float dip{0.0f};
        float dipRate{0.0f};
        bool wasGrounded{true};
        float fallSpeed{0.0f};
        float airSeconds{0.0f};
        uint32_t footfalls{0};
        uint32_t landings{0};
        float landSpeed{0.0f};
        float roll{0.0f};
        float kick{0.0f};
        float kickRate{0.0f};
        float fovOffset{0.0f};
        float bulletTimeWeight{0.0f};
        uint8_t shotCount{0};
        HeadWeightState weight{};
        bool wasVaulting{false};
        float lastKickSeconds{-1.0f};
    };

    // Advances the head motion by one frame: eases the eye to the stance's height, smooths steps
    // the controller climbs or drops (stairs), bobs it with the stride (walk, run and crouch
    // differ; a footfall plants with a small dip), dips on take-off and compresses on landing by
    // the fall speed, rolls into strafes, turns, slides and leans, leans into speed changes, trails
    // fast turns slightly, moves it with the lean, springs the camera recoil back, widens the view
    // while running and makes the head heavier (less bob, slower roll, more damped landings,
    // narrower view) in bullet-time.
    // state:   the head motion, updated.
    // input:   this frame's pawn state.
    // Returns: the camera pose.
    [[nodiscard]] HeadPose updateHeadMotion(HeadMotionState& state, const HeadMotionInput& input);

    // Marks the head motion idle (no local pawn this frame), so it restarts cleanly.
    // state: the head motion.
    void resetHeadMotion(HeadMotionState& state);

    // yaw:     camera yaw (look yaw plus HeadPose::yawOffset).
    // pitch:   camera pitch (look pitch plus HeadPose::pitchOffset).
    // roll:    camera roll (HeadPose::roll).
    // Returns: the camera's up vector for that orientation.
    [[nodiscard]] math::Vec3 rolledUp(float yaw, float pitch, float roll);

#if LOCKSTEP_DEV_TOOLS
    // Draws the head motion tuning controls of the Game dev panel.
    // state: the head motion.
    void headMotionDevSection(HeadMotionState& state);
#endif
} //namespace game

#endif //LOCKSTEP_GAME_PRESENTATION_HEAD_MOTION_HPP
