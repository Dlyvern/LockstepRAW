#include "Presentation/FirstPerson.hpp"

#include "Characters/FirstPersonBody.hpp"
#include "Characters/ViewModelOptic.hpp"
#include "Characters/ViewModelReload.hpp"
#include "GameState.hpp"
#include "Player/LocalPlayer.hpp"
#include "Player/PawnKick.hpp"
#include "Presentation/Mirrors.hpp"
#include "Presentation/Visuals.hpp"

#include <Lockstep/Platform/Input.hpp>
#include <Lockstep/Platform/Window.hpp>
#include <Lockstep/World/TimeDomains.hpp>

#include <algorithm>
#include <cmath>

namespace game
{
    namespace
    {
        namespace world = lockstep::world;

        using lockstep::app::EngineContext;
        using lockstep::app::FrameInfo;
        using lockstep::app::GameState;

        constexpr float k_bulletTimeThreshold{0.99f};
        constexpr float k_minLeanDistance{0.01f};

        math::Vec3 rightOf(const math::Vec3& forward)
        {
            return math::Vec3{-forward.z, 0.0f, forward.x};
        }
    } //namespace

    ShownWeapon shownWeapon(const GameState& state)
    {
        const LocalControls& controls = state.local.controls;
        const float raise = state.pawnCharacters ? state.pawnCharacters->viewModel.tuning.raiseSeconds : 0.3f;
        const float left = controls.switchSeconds;

        if (left <= 0.0f)
        {
            return ShownWeapon{.weapon = controls.weapon, .raiseSeconds = raise};
        }

        const float draw = weaponStats(state.settings.weapons, controls.weapon).drawSeconds;

        if (left > draw)
        {
            return ShownWeapon{.weapon = controls.previousWeapon,
                .switching = ViewModelSwitch{.isHolstering = true,
                    .seconds = weaponStats(state.settings.weapons, controls.previousWeapon).holsterSeconds,
                    .left = left - draw},
                .isSwitching = true,
                .raiseSeconds = raise};
        }

        return ShownWeapon{.weapon = controls.weapon,
            .switching = ViewModelSwitch{.isDrawing = true, .seconds = draw, .left = left},
            .isSwitching = true,
            .raiseSeconds = raise};
    }

    FirstPersonLook presentedLook(GameState& state, const EngineContext& engine, const FrameInfo& frameInfo)
    {
        GameState::Local& local = state.local;
        const bool isLooking = engine.window.isCursorCaptured() && !state.flyCamera.isActive;

        if (frameInfo.ticksCompleted != local.lookTicks || !isLooking)
        {
            local.pendingLook = math::Vec2{0.0f, 0.0f};
            local.lookTicks = frameInfo.ticksCompleted;
        }
        else
        {
            local.pendingLook = local.pendingLook + frameInfo.input.mouseDelta();
        }

        const ControlSettings& controls = state.settings.controls;
        const float sensitivity = lookSensitivity(controls, local.command.aim, local.controls.opticSensitivityScale);

        return FirstPersonLook{.yaw = local.command.yaw - local.pendingLook.x * sensitivity,
            .pitch = math::clamp(local.command.pitch - local.pendingLook.y * sensitivity, -controls.maxPitch,
                controls.maxPitch)};
    }

    bool isLocalSprinting(const GameState& state, const EngineContext& engine)
    {
        const PawnMotion& motion = localPawnMotion(state, engine);

        return isRunningCommand(state.local.command, motion)
            && math::length(motion.planarVelocity) > state.settings.pawns.walkSpeed;
    }

    HeadPose presentHead(GameState& state, const EngineContext& engine, const math::Transform& localPawn,
        const FirstPersonLook& look, float playerSeconds)
    {
        const PawnMotion& motion = localPawnMotion(state, engine);
        const PawnLook pawnLook = localPawnLook(state, engine);
        const math::Vec3 foot = pawnFootPosition(localPawn);
        const PawnSettings& pawns = state.settings.pawns;
        const float uprightEye = localPawn.scale.y - k_pawnEyeBelowTop;
        const math::Vec3 leanOffset = eyePosition(localPawn) - (foot + math::Vec3{0.0f, uprightEye, 0.0f});
        const float lean = math::dot(leanOffset, rightOf(lookDirection(look.yaw, 0.0f)))
            / std::max(pawns.leanDistance, k_minLeanDistance);

        return updateHeadMotion(state.headMotion,
            HeadMotionInput{.foot = foot,
                .eyeHeight = uprightEye,
                .leanOffset = leanOffset,
                .lean = math::clamp(lean, -1.0f, 1.0f),
                .yaw = look.yaw,
                .planarVelocity = motion.planarVelocity,
                .verticalSpeed = motion.verticalSpeed,
                .isGrounded = motion.isGrounded,
                .isCrouched = motion.stance != PawnStance::Stand,
                .isSliding = isSliding(motion),
                .isVaulting = isVaulting(motion),
                .kickSeconds = kickSeconds(motion, engine.timeDomains.delta(world::TimeDomain::Player)),
                .kickImpactSeconds = pawns.kick.impactSeconds - engine.timeDomains.delta(world::TimeDomain::Player),
                .isAiming = pawnLook.isAiming,
                .isSprinting = isLocalSprinting(state, engine),
                .isBulletTime = engine.timeDomains.scale(world::TimeDomain::World) < k_bulletTimeThreshold,
                .walkSpeed = pawns.walkSpeed,
                .runSpeed = pawns.runSpeed,
                .strafeSpeed = pawns.strafeSpeed,
                .shotCount = state.local.command.shotCount,
                .canKick = pawnLook.isArmed && !pawnLook.isLowered && !isHoldingToy(state, engine),
                .deltaSeconds = playerSeconds});
    }

    void presentViewModel(GameState& state, EngineContext& engine, const std::optional<math::Vec3>& eye,
        const HeadPose& head, const FirstPersonLook& look, float playerSeconds)
    {
        if (!state.pawnCharacters || firstPersonModel(*state.pawnCharacters, WeaponKind::Kriss) == nullptr)
        {
            return;
        }

        PawnCharacters& characters = *state.pawnCharacters;
        const PawnLook pawnLook = localPawnLook(state, engine);
        const ShownWeapon shown = shownWeapon(state);
        const FirstPersonModel& model = *firstPersonModel(characters, shown.weapon);

        if (!eye || !pawnLook.isArmed || state.flyCamera.isActive)
        {
            hideViewModel(characters.viewModel);
            return;
        }

        const PawnMotion& motion = localPawnMotion(state, engine);
        const math::Vec3 forward = lookDirection(look.yaw, 0.0f);
        const math::Vec3 right = rightOf(forward);
        const std::size_t index = localPawnIndex(engine);
        const PlayerStatus& status = state.local.statuses[index];
        const bool isReloading = status.isReloading;
        const PlayerReload& reload = state.weaponFx.reloads.players[index];
        const bool isEmptyReload = reload.isActive ? reload.kind == ReloadKind::Empty : status.magazine <= 0;
        const bool isClipReload = isReloading && hasReloadClips(model) && !shown.switching.isHolstering
            && !shown.switching.isDrawing;
        const bool isSwitchClip = (shown.switching.isHolstering || shown.switching.isDrawing) && hasSwitchClips(model);
        const bool isSwitchLowered = !hasSwitchClips(model)
            && (shown.switching.isHolstering || (shown.switching.isDrawing && shown.switching.left > shown.raiseSeconds));
        const OpticModel* optic = weaponOptic(state.optics, model.weapon);
        prepareViewModelOptic(characters.viewModel, model, optic, state.optics);
        state.local.controls.opticSensitivityScale = opticSensitivityScale(state.optics, optic);
        const WeaponStats& stats = weaponStats(state.settings.weapons, shown.weapon);
        drawViewModel(engine.renderer, model, characters.viewModel,
            ViewModelInput{.eye = *eye,
                .yaw = look.yaw,
                .pitch = look.pitch,
                .cameraPitchOffset = head.pitchOffset,
                .cameraYawOffset = head.yawOffset,
                .cameraRoll = head.roll,
                .bobPhase = head.bobPhase,
                .bobStrength = head.bobStrength,
                .velocity = {math::dot(motion.planarVelocity, right), math::dot(motion.planarVelocity, forward)},
                .verticalSpeed = motion.verticalSpeed,
                .isGrounded = motion.isGrounded || (isMantling(motion) && !isVaulting(motion)),
                .strafeSpeed = state.settings.pawns.strafeSpeed,
                .isCrouched = motion.stance != PawnStance::Stand,
                .isAiming = pawnLook.isAiming && !isReloading && !isSwitchClip && !isSwitchLowered,
                .isLowered = pawnLook.isLowered || (isReloading && !isClipReload && !isSwitchClip) || isSwitchLowered,
                .isVaulting = isVaulting(motion),
                .isKicking = isKicking(motion),
                .isSprinting = isLocalSprinting(state, engine) && !isClipReload && !isSwitchClip,
                .wantsHoldBreath = state.local.command.sprint,
                .canFire = !isHoldingToy(state, engine) && !isReloading && !shown.isSwitching,
                .shotCount = state.local.command.shotCount,
                .reload = ViewModelReload{.isActive = isClipReload,
                    .isEmpty = isEmptyReload,
                    .seconds = isEmptyReload ? stats.ammo.emptyReloadSeconds : stats.ammo.reloadSeconds},
                .switching = shown.switching,
                .deltaSeconds = playerSeconds});
        const float worldFov = firstPersonCamera(*eye, forward, viewModelZoom(characters.viewModel),
            math::Vec3{0.0f, 1.0f, 0.0f}, head.fovOffset).verticalFov;
        presentViewModelOptic(engine.renderer, model, characters.viewModel, optic,
            state.optics, worldFov,
            lockstep::render::k_allRenderLayers & ~(k_mirrorOnlyLayer | k_firstPersonBodyLayer));
    }
} //namespace game
