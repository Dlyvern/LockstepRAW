#ifndef LOCKSTEP_GAME_CHARACTERS_VIEW_MODEL_HPP
#define LOCKSTEP_GAME_CHARACTERS_VIEW_MODEL_HPP

#include "Characters/CharacterModel.hpp"
#include "Characters/WeaponSway.hpp"
#include "Weapons/WeaponKind.hpp"

#include <Lockstep/Anim/SkinnedModel.hpp>
#include <Lockstep/Core/Math.hpp>
#include <Lockstep/Render/CharacterSurface.hpp>
#include <Lockstep/Core/Result.hpp>
#include <Lockstep/Core/Transform.hpp>

#include <cstdint>
#include <optional>
#include <string_view>

namespace lockstep::render
{
    class Renderer;
} //namespace lockstep::render

namespace game
{
    namespace math = lockstep::math;

    // A reload the viewmodel plays.
    // isActive: a reload runs.
    // isEmpty:  from an empty magazine (the clip with the charging handle).
    // seconds:  Player seconds the whole reload takes (the clip is stretched to it).
    struct ViewModelReload
    {
        bool isActive{false};
        bool isEmpty{false};
        float seconds{2.0f};
    };

    // A weapon switch the viewmodel plays: the old weapon is put away (Holster clip, or lowered
    // without one), then the new one is brought up (Draw clip, or raised without one).
    // isHolstering: the weapon shown is being put away.
    // isDrawing:    the weapon shown is being brought up.
    // seconds:      Player seconds the current phase takes (its clip is stretched to it).
    // left:         Player seconds left of the current phase.
    struct ViewModelSwitch
    {
        bool isHolstering{false};
        bool isDrawing{false};
        float seconds{0.5f};
        float left{0.0f};
    };

    // Where the viewmodel sits in one pose, in view space (x right, y up, -z forward, metres):
    // the rotation and the position of the model's origin. The constants for the SWAT KRISS model
    // (ViewModel.cpp) were computed offline from swat_fp_kriss.glb at a 55 degree viewmodel field
    // of view: aiming down sights puts the rear aperture and the front post on the view axis, the
    // rear sight 0.20 m in front of the eye; the ready stance (FPAim pose) puts the receiver at
    // about (0.40, -0.45) in NDC, 0.48 m deep, muzzle turned towards the centre, nothing nearer
    // than 0.28 m; the lowered stance (FPLow pose) drops it to about (0.45, -0.70), muzzle down,
    // nothing nearer than 0.35 m. The left forearm stays at the bottom edge in both.
    // rotation: model-to-view rotation.
    // origin:   view-space position of the model's origin.
    struct ViewModelPlacement
    {
        math::Quat rotation{};
        math::Vec3 origin{0.0f, 0.0f, 0.0f};
    };

    // The per-weapon placement data of a first-person model, in model space (FPAim pose).
    // ready:      where the model sits with the weapon up.
    // sights:     where it sits aiming down the iron sights (rear and front sight on the view axis).
    // anchor:     the point the lowered and running poses and the motion turn about (the weapon).
    // rearSight, frontSight, muzzle: points on the weapon (ViewModelSights).
    struct ViewModelRig
    {
        ViewModelPlacement ready{};
        ViewModelPlacement sights{};
        math::Vec3 anchor{0.0f, 0.0f, 0.0f};
        math::Vec3 rearSight{0.0f, 0.0f, 0.0f};
        math::Vec3 frontSight{0.0f, 0.0f, -0.1f};
        math::Vec3 muzzle{0.0f, 0.0f, -0.2f};
    };

    // The first-person arms-and-weapon model and its static pose. The lowered stance turns this
    // pose (ViewModelTuning::lowered*), so the hands keep the same grip.
    // character: the arms model.
    // aimClip:   the raised pose (ready stance and aiming down sights).
    // tacticalReloadClip, emptyReloadClip: the reload clips (ReloadTactical, ReloadEmpty), if the
    //            model has them; without them a reload lowers the weapon instead.
    // drawClip, holsterClip: the weapon switch clips (Draw, Holster), if the model has them.
    // weapon:    the loadout weapon it shows.
    // rig:       its placements and sight points (viewModelRig()).
    struct FirstPersonModel
    {
        CharacterModel character;
        uint32_t aimClip{0};
        std::optional<uint32_t> tacticalReloadClip{};
        std::optional<uint32_t> emptyReloadClip{};
        std::optional<uint32_t> drawClip{};
        std::optional<uint32_t> holsterClip{};
        WeaponKind weapon{WeaponKind::Kriss};
        ViewModelRig rig{};
    };

    // Feel of the viewmodel's procedural motion, editable from the dev panel.
    // aimSeconds:     time to go into or out of aiming down sights.
    // raiseSeconds:   time to raise or lower the weapon (pose crossfade and placement).
    // vaultSeconds:   time to swing the weapon down and aside for a vault (the left hand plants
    //                 on the obstacle) and back.
    // kickLift:       upward speed (m/s) a kick gives the weapon (the arms brace as the leg goes).
    // kickPitch:      muzzle climb rate (rad/s) a kick gives it.
    // aimDip:         how far the weapon dips (down and back, metres) halfway into aiming.
    // swayLag:        radians the weapon lags per radian of look.
    // swayLimit:      largest lag, in radians.
    // swayStiffness:  spring pulling the lag back, 1/s^2.
    // swayDamping:    damping of that spring, 1/s (below critical, so the weapon overshoots a
    //                 little as it catches up: a heavy rifle).
    // swayRoll:       roll per radian of sideways sway (the weapon cants as it trails a turn).
    // bobAmount:      sideways bob in metres at walking speed (vertical is two thirds of it).
    // bobRoll:        roll of the bob in radians at walking speed.
    // kick:           how each shot throws the weapon (climb, sideways, roll, back).
    // breath:         breathing sway, stamina and the held breath.
    // strafeTilt:     roll into a strafe at strafe speed, radians.
    // strafeShift:    sideways lag at strafe speed, metres (the weapon trails the move).
    // moveKick:       metres/s of weapon lag per m/s of velocity change (back and down when
    //                 speeding up, forward when stopping).
    // moveDip:        vertical part of that lag, relative to its forward part.
    // moveStiffness, moveDamping: the spring of the movement reaction (it overshoots a little,
    //                 so stopping ends in a short settle).
    // moveLimit:      largest movement offset, metres.
    // movePitch:      muzzle dip rate (rad/s) per m/s of forward velocity change (starting and
    //                 stopping both dip it).
    // moveYaw:        muzzle trail rate (rad/s) per m/s of sideways velocity change.
    // moveAngleLimit: largest muzzle dip or trail from movement, radians.
    // jumpDrop:       downward speed (m/s) of the weapon when a jump leaves the ground.
    // jumpPitch:      muzzle dip rate (rad/s) at take-off.
    // landDrop:       downward speed (m/s) of the weapon per m/s of fall speed on landing.
    // landPitch:      muzzle dip rate (rad/s) per m/s of fall speed on landing.
    // landMinSpeed:   falls slower than this (m/s) don't dip the weapon.
    // airStiffness, airDamping: the spring of the jump and landing dips.
    // airLimit:       largest jump or landing drop, metres (the pitch is limited to twice that in
    //                 radians).
    // aimDamping:     multiplier of look sway, bob and movement reactions while aiming (the kick
    //                 and the breathing have their own aiming amounts).
    // readyOffset:    extra view-space offset with the weapon up (ready or lowered).
    // aimOffset:      extra view-space offset while aiming down sights.
    // loweredPitch, loweredYaw, loweredRoll: how the lowered pose turns the ready pose, radians
    //                 (muzzle down, muzzle toward the left, cant).
    // loweredPivot:   view-space point (near the stock) the ready pose turns about when lowered.
    // loweredOffset:  extra view-space offset of the lowered pose.
    // sprintSeconds:  time to swing into or out of the running pose.
    // sprintPitch, sprintYaw, sprintRoll: how the running pose turns the ready pose about the
    //                 lowered pivot, radians (muzzle down, across the body, canted).
    // sprintOffset:   extra view-space offset of the running pose.
    // sprintSwing:    sideways swing per step while running, radians.
    // sprintBounce:   vertical bounce per step while running, metres.
    // sprintRollSwing: roll swing per step while running, radians.
    struct ViewModelTuning
    {
        float aimSeconds{0.15f};
        float raiseSeconds{0.3f};
        float vaultSeconds{0.1f};
        float kickLift{0.45f};
        float kickPitch{1.2f};
        float aimDip{0.012f};
        float swayLag{0.16f};
        float swayLimit{0.08f};
        float swayStiffness{85.0f};
        float swayDamping{11.0f};
        float swayRoll{0.6f};
        float bobAmount{0.006f};
        float bobRoll{0.008f};
        WeaponKickTuning kick{};
        BreathTuning breath{};
        float strafeTilt{0.08f};
        float strafeShift{0.012f};
        float moveKick{0.02f};
        float moveDip{0.6f};
        float moveStiffness{75.0f};
        float moveDamping{9.0f};
        float moveLimit{0.035f};
        float movePitch{0.06f};
        float moveYaw{0.05f};
        float moveAngleLimit{0.06f};
        float jumpDrop{0.3f};
        float jumpPitch{0.5f};
        float landDrop{0.06f};
        float landPitch{0.1f};
        float landMinSpeed{1.5f};
        float airStiffness{110.0f};
        float airDamping{10.0f};
        float airLimit{0.05f};
        float aimDamping{0.35f};
        math::Vec3 readyOffset{0.0f, 0.0f, 0.0f};
        math::Vec3 aimOffset{0.0f, 0.0f, 0.0f};
        float loweredPitch{0.3f};
        float loweredYaw{0.35f};
        float loweredRoll{0.1f};
        math::Vec3 loweredPivot{0.17f, -0.07f, -0.23f};
        math::Vec3 loweredOffset{-0.02f, 0.0f, 0.0f};
        float sprintSeconds{0.16f};
        float sprintPitch{0.35f};
        float sprintYaw{0.6f};
        float sprintRoll{0.5f};
        math::Vec3 sprintOffset{0.0f, -0.02f, 0.03f};
        float sprintSwing{0.07f};
        float sprintBounce{0.016f};
        float sprintRollSwing{0.06f};
    };

    // Aiming through an optic mounted on the viewmodel's rail (Characters/ViewModelOptic.hpp).
    // sights:      the aiming placement that puts the eye on the optic's axis at its eye relief;
    //              replaces the iron sights placement.
    // swayScale:   multiplier on the breathing sway while aiming (a magnified optic shows more of it).
    // isMagnified: the optic magnifies (the view around it may soften while aiming).
    // axis:        where the optic points this frame, unit world direction as the world camera
    //              shows it (set after drawViewModel()); shots aimed down sights follow it.
    struct ViewModelOpticAim
    {
        ViewModelPlacement sights{};
        float swayScale{1.0f};
        bool isMagnified{false};
        std::optional<math::Vec3> axis{};
    };

    // The local player's viewmodel: its animated instance and the state of its procedural motion.
    // Presentation only, never replicated.
    // instance:   the arms' pose and animator.
    // aimBlend:   0 not aiming, 1 aiming down sights, moved linearly by aimSeconds.
    // raiseBlend: 0 lowered, 1 up, moved linearly by raiseSeconds.
    // sprintBlend: 0 not running, 1 in the running pose, moved linearly by sprintSeconds.
    // sway:       look lag as (yaw, pitch) radians, and its rate.
    // kick:       the weapon kick of the shots.
    // breath:     breathing, stamina and the held breath.
    // breathSway: this frame's breathing sway as (yaw, pitch) radians.
    // shotCount:  the local shot counter last seen, so each new shot kicks once.
    // tilt, tiltRate: strafe roll in radians, and its rate.
    // moveOffset, moveRate: view-space lag from velocity changes and strafing, and its rate.
    // moveAngle, moveAngleRate: (pitch, yaw) muzzle dip and trail from velocity changes, radians,
    //               and its rate.
    // air, airRate: (drop in metres, pitch in radians) from jumping and landing, and its rate.
    // wasGrounded:  on the ground last frame.
    // fallSpeed:    fastest fall since leaving the ground, m/s.
    // lastVelocity: world planar velocity of the previous frame (x, z; m/s).
    // lastYaw, lastPitch: the look of the previous frame.
    // isDrawn:    drawn last frame; false restarts the motion state.
    // wasVaulting: the vault swing is still returning (raises at vaultSeconds).
    // wasKicking: kicking last frame.
    // isReloadPlaying: a reload clip plays.
    // switchClip: the switch clip playing: 0 none, 1 Holster, 2 Draw.
    // weapon:     the weapon of the model drawn last frame (a change restarts the motion state).
    // rig:        that model's placements and sight points.
    // view:       where the model was placed last frame, in view space (before the camera turn).
    // camera:     the camera rotation it was placed with (look, head motion and roll).
    // eye:        the camera position it was placed with.
    // opticAim:   aiming through the mounted optic, or nothing for iron sights.
    // tuning:     feel settings.
    // surface:    the character surface (rain soak) drawn over the arms this frame.
    struct ViewModelState
    {
        lockstep::anim::SkinnedInstance instance{};
        float aimBlend{0.0f};
        float raiseBlend{1.0f};
        float sprintBlend{0.0f};
        math::Vec2 sway{0.0f, 0.0f};
        math::Vec2 swayRate{0.0f, 0.0f};
        WeaponKickState kick{};
        BreathState breath{};
        math::Vec2 breathSway{0.0f, 0.0f};
        uint8_t shotCount{0};
        float tilt{0.0f};
        float tiltRate{0.0f};
        math::Vec3 moveOffset{0.0f, 0.0f, 0.0f};
        math::Vec3 moveRate{0.0f, 0.0f, 0.0f};
        math::Vec2 moveAngle{0.0f, 0.0f};
        math::Vec2 moveAngleRate{0.0f, 0.0f};
        math::Vec2 air{0.0f, 0.0f};
        math::Vec2 airRate{0.0f, 0.0f};
        bool wasGrounded{true};
        float fallSpeed{0.0f};
        math::Vec2 lastVelocity{0.0f, 0.0f};
        float lastYaw{0.0f};
        float lastPitch{0.0f};
        bool isDrawn{false};
        bool wasVaulting{false};
        bool wasKicking{false};
        bool isReloadPlaying{false};
        uint8_t switchClip{0};
        WeaponKind weapon{WeaponKind::Kriss};
        ViewModelRig rig{};
        math::Transform view{};
        math::Quat camera{};
        math::Vec3 eye{0.0f, 0.0f, 0.0f};
        std::optional<ViewModelOpticAim> opticAim{};
        ViewModelTuning tuning{};
        lockstep::render::CharacterSurfaceId surface{lockstep::render::k_noCharacterSurface};
    };

    // What drives the viewmodel this frame.
    // eye:          the camera position.
    // yaw, pitch:   the look (radians, as in PlayerCommand); sway follows their changes.
    // cameraPitchOffset, cameraYawOffset, cameraRoll: the camera's presentation-only kick, lag
    //               and roll (HeadPose), so the weapon stays fixed on screen.
    // bobPhase:     the head's step cycle (HeadPose::bobPhase), so weapon and camera bob together.
    // bobStrength:  bob size relative to walking (HeadPose::bobStrength).
    // velocity:     the pawn's planar velocity relative to the look yaw, m/s (x right, y forward).
    // verticalSpeed: the pawn's upward speed, m/s (jump and landing dips).
    // isGrounded:   the pawn is on the ground.
    // strafeSpeed:  the strafe speed the tilt is scaled by.
    // isCrouched:   crouched or sliding (less recoil).
    // isAiming:     the player aims down sights.
    // isLowered:    the weapon is lowered.
    // isVaulting:   the player vaults (the weapon swings down and aside quickly, freeing the
    //               left hand; no shot fires meanwhile).
    // isKicking:    the player kicks (the weapon jolts up as the leg goes out).
    // isSprinting:  the player runs (the weapon swings into the running pose; drains stamina).
    // wantsHoldBreath: the hold-breath key (Shift) is down; holds the breath while aiming.
    // canFire:      a click fires the weapon (false while it throws a held toy); with the weapon
    //               lowered no shot kicks either.
    // shotCount:    the local shot counter; each increase kicks when the weapon fires.
    // reload:       the running reload, played as a clip.
    // switching:    the running weapon switch, played as clips (it wins over a reload).
    // deltaSeconds: Player time since the previous frame.
    struct ViewModelInput
    {
        math::Vec3 eye{0.0f, 0.0f, 0.0f};
        float yaw{0.0f};
        float pitch{0.0f};
        float cameraPitchOffset{0.0f};
        float cameraYawOffset{0.0f};
        float cameraRoll{0.0f};
        float bobPhase{0.0f};
        float bobStrength{0.0f};
        math::Vec2 velocity{0.0f, 0.0f};
        float verticalSpeed{0.0f};
        bool isGrounded{true};
        float strafeSpeed{3.4f};
        bool isCrouched{false};
        bool isAiming{false};
        bool isLowered{false};
        bool isVaulting{false};
        bool isKicking{false};
        bool isSprinting{false};
        bool wantsHoldBreath{false};
        bool canFire{true};
        uint8_t shotCount{0};
        ViewModelReload reload{};
        ViewModelSwitch switching{};
        float deltaSeconds{0.0f};
    };

    // weapon:  a loadout weapon.
    // Returns: its first-person model's placements and sight points: the KRISS' computed offline
    //          from swat_fp_kriss.glb (see ViewModelPlacement); the M4's and Glock's models put the
    //          eye at the origin looking along -z (their ready placement is identity), and their
    //          sights placement puts the rear and front sight on the view axis at an eye relief.
    [[nodiscard]] ViewModelRig viewModelRig(WeaponKind weapon);

    // Finds the first-person pose of an arms model and logs where its head sits in the aiming
    // pose (a check of the placement constants).
    // character: the loaded arms model.
    // aimClip:   name of the aiming pose.
    // weapon:    the loadout weapon it shows.
    // Returns:   the model, or InvalidArgument if the clip is missing.
    [[nodiscard]] lockstep::Result<FirstPersonModel> createFirstPersonModel(CharacterModel&& character,
        std::string_view aimClip, WeaponKind weapon);

    // Animates the viewmodel (raise/lower crossfade, look sway with overshoot and cant,
    // breathing (stamina, held breath), walk bob, strafe tilt, lag and muzzle dip when starting and
    // stopping, dips on jumping and landing, running pose swinging with the steps, aim-in, weapon
    // kick) and
    // queues it on render::k_viewModelLayer, placed relative to the camera. Sway turns it about the
    // eye; bob, dip and recoil together move it at most a few centimetres, so it never comes near
    // the eye.
    // renderer: the renderer.
    // model:    the arms model.
    // state:    the viewmodel state, updated.
    // input:    this frame's camera, movement, aim and shots.
    void drawViewModel(lockstep::render::Renderer& renderer, const FirstPersonModel& model, ViewModelState& state,
        const ViewModelInput& input);

    // How far the view is zoomed for aiming down sights.
    // state:   the viewmodel state.
    // Returns: 0 (no zoom) to 1 (fully aimed), eased.
    [[nodiscard]] float viewModelZoom(const ViewModelState& state);

    // Marks the viewmodel hidden this frame, so it restarts cleanly when shown again.
    // state: the viewmodel state.
    void hideViewModel(ViewModelState& state);

#if LOCKSTEP_DEV_TOOLS
    // Draws the viewmodel tuning controls of the Game dev panel.
    // state: the viewmodel state.
    void viewModelDevSection(ViewModelState& state);
#endif
} //namespace game

#endif //LOCKSTEP_GAME_CHARACTERS_VIEW_MODEL_HPP
