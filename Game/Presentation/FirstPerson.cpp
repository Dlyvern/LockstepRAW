#include "Presentation/FirstPerson.hpp"

#include "Characters/ViewModelReload.hpp"
#include "GameState.hpp"
#include "Player/LocalPlayer.hpp"
#include "Player/PawnKick.hpp"

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
        const float sensitivity = lookSensitivity(controls, local.command.aim);

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
        if (!state.pawnCharacters || !state.pawnCharacters->firstPerson)
        {
            return;
        }

        PawnCharacters& characters = *state.pawnCharacters;
        const PawnLook pawnLook = localPawnLook(state, engine);

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
        const PlayerReload& shown = state.weaponFx.reloads.players[index];
        const bool isEmptyReload = shown.isActive ? shown.kind == ReloadKind::Empty : status.magazine <= 0;
        const bool isClipReload = isReloading && hasReloadClips(*characters.firstPerson);
        drawViewModel(engine.renderer, *characters.firstPerson, characters.viewModel,
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
                .isAiming = pawnLook.isAiming && !isReloading,
                .isLowered = pawnLook.isLowered || (isReloading && !isClipReload),
                .isVaulting = isVaulting(motion),
                .isKicking = isKicking(motion),
                .isSprinting = isLocalSprinting(state, engine) && !isClipReload,
                .wantsHoldBreath = state.local.command.sprint,
                .canFire = !isHoldingToy(state, engine) && !isReloading,
                .shotCount = state.local.command.shotCount,
                .reload = ViewModelReload{.isActive = isClipReload,
                    .isEmpty = isEmptyReload,
                    .seconds = isEmptyReload ? state.settings.ammo.emptyReloadSeconds : state.settings.ammo.reloadSeconds},
                .deltaSeconds = playerSeconds});
    }
} //namespace game
