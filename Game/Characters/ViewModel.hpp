#ifndef LOCKSTEP_GAME_CHARACTERS_VIEW_MODEL_HPP
#define LOCKSTEP_GAME_CHARACTERS_VIEW_MODEL_HPP

#include "Characters/CharacterModel.hpp"

#include <Lockstep/Anim/SkinnedModel.hpp>
#include <Lockstep/Core/Math.hpp>
#include <Lockstep/Core/Result.hpp>

#include <cstdint>
#include <string_view>

namespace lockstep::render
{
    class Renderer;
} //namespace lockstep::render

namespace game
{
    namespace math = lockstep::math;

    // The first-person arms-and-weapon model and its static pose. The lowered stance turns this
    // pose (ViewModelTuning::lowered*), so the hands keep the same grip.
    // character: the arms model.
    // aimClip:   the raised pose (ready stance and aiming down sights).
    struct FirstPersonModel
    {
        CharacterModel character;
        uint32_t aimClip{0};
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

    // Feel of the viewmodel's procedural motion, editable from the dev panel.
    // aimSeconds:     time to go into or out of aiming down sights.
    // raiseSeconds:   time to raise or lower the weapon (pose crossfade and placement).
    // aimDip:         how far the weapon dips (down and back, metres) halfway into aiming.
    // swayLag:        radians the weapon lags per radian of look.
    // swayLimit:      largest lag, in radians.
    // swayStiffness:  spring pulling the lag back, 1/s^2.
    // swayDamping:    damping of that spring, 1/s.
    // bobAmount:      sideways bob in metres at walking speed (vertical is two thirds of it).
    // bobRoll:        roll of the bob in radians at walking speed.
    // breatheAmount:  idle breathing sway, radians.
    // breatheRate:    breaths per second.
    // recoilKick:     muzzle climb speed one shot adds, rad/s.
    // recoilBack:     metres the weapon moves back per radian of climb.
    // recoilStiffness, recoilDamping: the recoil spring.
    // crouchRecoilScale: recoil multiplier while crouched or sliding.
    // aimDamping:     multiplier of sway, bob and recoil while aiming.
    // readyOffset:    extra view-space offset with the weapon up (ready or lowered).
    // aimOffset:      extra view-space offset while aiming down sights.
    // loweredPitch, loweredYaw, loweredRoll: how the lowered pose turns the ready pose, radians
    //                 (muzzle down, muzzle toward the left, cant).
    // loweredPivot:   view-space point (near the stock) the ready pose turns about when lowered.
    // loweredOffset:  extra view-space offset of the lowered pose.
    struct ViewModelTuning
    {
        float aimSeconds{0.15f};
        float raiseSeconds{0.3f};
        float aimDip{0.012f};
        float swayLag{0.12f};
        float swayLimit{0.06f};
        float swayStiffness{110.0f};
        float swayDamping{16.0f};
        float bobAmount{0.005f};
        float bobRoll{0.008f};
        float breatheAmount{0.004f};
        float breatheRate{0.25f};
        float recoilKick{0.9f};
        float recoilBack{1.2f};
        float recoilStiffness{320.0f};
        float recoilDamping{26.0f};
        float crouchRecoilScale{0.7f};
        float aimDamping{0.35f};
        math::Vec3 readyOffset{0.0f, 0.0f, 0.0f};
        math::Vec3 aimOffset{0.0f, 0.0f, 0.0f};
        float loweredPitch{0.3f};
        float loweredYaw{0.35f};
        float loweredRoll{0.1f};
        math::Vec3 loweredPivot{0.17f, -0.07f, -0.23f};
        math::Vec3 loweredOffset{-0.02f, 0.0f, 0.0f};
    };

    // The local player's viewmodel: its animated instance and the state of its procedural motion.
    // Presentation only, never replicated.
    // instance:   the arms' pose and animator.
    // aimBlend:   0 not aiming, 1 aiming down sights, moved linearly by aimSeconds.
    // raiseBlend: 0 lowered, 1 up, moved linearly by raiseSeconds.
    // sway:       look lag as (yaw, pitch) radians, and its rate.
    // breathePhase: idle breathing cycle, radians.
    // recoil:     muzzle climb in radians, and its rate.
    // shotCount:  the local shot counter last seen, so each new shot kicks once.
    // lastYaw, lastPitch: the look of the previous frame.
    // isDrawn:    drawn last frame; false restarts the motion state.
    // tuning:     feel settings.
    struct ViewModelState
    {
        lockstep::anim::SkinnedInstance instance{};
        float aimBlend{0.0f};
        float raiseBlend{1.0f};
        math::Vec2 sway{0.0f, 0.0f};
        math::Vec2 swayRate{0.0f, 0.0f};
        float breathePhase{0.0f};
        float recoil{0.0f};
        float recoilRate{0.0f};
        uint8_t shotCount{0};
        float lastYaw{0.0f};
        float lastPitch{0.0f};
        bool isDrawn{false};
        ViewModelTuning tuning{};
    };

    // What drives the viewmodel this frame.
    // eye:          the camera position.
    // yaw, pitch:   the look (radians, as in PlayerCommand); sway follows their changes.
    // cameraPitchOffset, cameraRoll: the camera's presentation-only kick and roll (HeadPose), so
    //               the weapon stays fixed on screen.
    // bobPhase:     the head's step cycle (HeadPose::bobPhase), so weapon and camera bob together.
    // bobStrength:  bob size relative to walking (HeadPose::bobStrength).
    // isCrouched:   crouched or sliding (less recoil).
    // isAiming:     the player aims down sights.
    // isLowered:    the weapon is lowered.
    // canFire:      a click fires the weapon (false while it throws a held toy); with the weapon
    //               lowered no shot kicks either.
    // shotCount:    the local shot counter; each increase kicks when the weapon fires.
    // deltaSeconds: Player time since the previous frame.
    struct ViewModelInput
    {
        math::Vec3 eye{0.0f, 0.0f, 0.0f};
        float yaw{0.0f};
        float pitch{0.0f};
        float cameraPitchOffset{0.0f};
        float cameraRoll{0.0f};
        float bobPhase{0.0f};
        float bobStrength{0.0f};
        bool isCrouched{false};
        bool isAiming{false};
        bool isLowered{false};
        bool canFire{true};
        uint8_t shotCount{0};
        float deltaSeconds{0.0f};
    };

    // Finds the first-person pose of an arms model and logs where its head sits in the aiming
    // pose (a check of the placement constants).
    // character: the loaded arms model.
    // aimClip:   name of the aiming pose.
    // Returns:   the model, or InvalidArgument if the clip is missing.
    [[nodiscard]] lockstep::Result<FirstPersonModel> createFirstPersonModel(CharacterModel&& character,
        std::string_view aimClip);

    // Animates the viewmodel (raise/lower crossfade, look sway, idle breathing, walk bob, aim-in,
    // recoil) and
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
