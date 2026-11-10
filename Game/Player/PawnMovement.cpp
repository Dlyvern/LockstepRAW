#include "Player/PawnMovement.hpp"

#include "Player/PawnKick.hpp"
#include "Player/PawnLean.hpp"
#include "Player/PawnMantle.hpp"
#include "Player/PawnVault.hpp"

#include <Lockstep/Physics/PhysicsWorld.hpp>

#include <algorithm>
#include <cmath>
#include <limits>

namespace game
{
    namespace
    {
        namespace physics = lockstep::physics;

        constexpr math::Vec3 k_upAxis{0.0f, 1.0f, 0.0f};
        constexpr float k_minSpeed{0.01f};
        constexpr float k_minMoveLength{0.001f};
        constexpr float k_slideBlockedFraction{0.3f};

        math::Vec3 forwardOf(float yaw)
        {
            return math::Vec3{-std::sin(yaw), 0.0f, -std::cos(yaw)};
        }

        math::Vec3 rightOf(float yaw)
        {
            return math::Vec3{std::cos(yaw), 0.0f, -std::sin(yaw)};
        }

        float ellipseSpeed(const math::Vec2& direction, float sideSpeed, float forwardSpeed)
        {
            const float side = direction.x / std::max(sideSpeed, k_minSpeed);
            const float forward = direction.y / std::max(forwardSpeed, k_minSpeed);

            return 1.0f / std::sqrt(side * side + forward * forward);
        }

        float landingScale(const PawnMotion& motion, const PawnSettings& settings, float delta)
        {
            if (motion.landingTicks == 0)
            {
                return 1.0f;
            }

            const float recoverTicks = std::max(static_cast<float>(secondsToTicks(settings.hardLandingMinSeconds, delta)),
                1.0f);
            const float hold = std::min(static_cast<float>(motion.landingTicks) / recoverTicks, 1.0f);

            return 1.0f - (1.0f - settings.hardLandingScale) * hold;
        }

        void landHard(PawnMotion& motion, const PawnSettings& settings, float impactSpeed, float delta)
        {
            if (impactSpeed < settings.hardLandingSpeed)
            {
                return;
            }

            const float severity = math::clamp((impactSpeed - settings.hardLandingSpeed)
                    / std::max(settings.hardLandingFullSpeed - settings.hardLandingSpeed, k_minSpeed), 0.0f, 1.0f);
            const float seconds = settings.hardLandingMinSeconds
                + (settings.hardLandingMaxSeconds - settings.hardLandingMinSeconds) * severity;
            motion.landingTicks = std::max(motion.landingTicks, secondsToTicks(seconds, delta));
            motion.planarVelocity = motion.planarVelocity * settings.hardLandingScale;
        }

        math::Vec3 wantedVelocity(const PlayerCommand& command, const PawnMotion& motion, const PawnSettings& settings,
            float delta)
        {
            math::Vec2 move = command.move;
            const float moveLength = math::length(move);

            if (moveLength < k_minMoveLength)
            {
                return math::Vec3{0.0f, 0.0f, 0.0f};
            }

            const bool isCrouched = motion.stance != PawnStance::Stand;
            const bool isRunning = isRunningCommand(command, motion);
            float speedCap = isAimingReady(command, motion) ? settings.aimSpeed : std::numeric_limits<float>::max();

            if (isCrouched)
            {
                speedCap = std::min(speedCap, settings.crouchSpeed);
            }

            const float runSpeed = command.isWeaponUp ? std::min(settings.runSpeed, settings.weaponUpRunSpeed)
                                                      : settings.runSpeed;
            const float forwardSpeed = std::min(speedCap, move.y < 0.0f ? settings.backSpeed
                                                     : (isRunning ? runSpeed : settings.walkSpeed));
            const float sideSpeed = std::min(speedCap, isRunning ? settings.sprintStrafeSpeed : settings.strafeSpeed);
            const math::Vec2 direction = move * (1.0f / moveLength);
            const float speed = std::min(moveLength, 1.0f) * ellipseSpeed(direction, sideSpeed, forwardSpeed)
                * landingScale(motion, settings, delta) * kickSpeedScale(motion, settings);

            return (forwardOf(command.yaw) * direction.y + rightOf(command.yaw) * direction.x) * speed;
        }

        math::Vec3 approach(const math::Vec3& current, const math::Vec3& target, float maxChange)
        {
            const math::Vec3 difference = target - current;
            const float distance = math::length(difference);

            if (distance <= maxChange)
            {
                return target;
            }

            return current + difference * (maxChange / distance);
        }

        float approach(float current, float target, float maxChange)
        {
            return current + math::clamp(target - current, -maxChange, maxChange);
        }

        math::Vec3 easeGroundVelocity(const math::Vec3& current, const math::Vec3& wanted,
            const PawnSettings& settings, float delta)
        {
            const float speed = math::length(current);

            if (speed < k_minSpeed)
            {
                return approach(current, wanted, settings.acceleration * delta);
            }

            const math::Vec3 along = current * (1.0f / speed);
            const float wantedAlong = math::dot(wanted, along);
            const math::Vec3 wantedSide = wanted - along * wantedAlong;
            const bool isSpeedingUp = wantedAlong > speed;
            const float speedUpRate = speed < settings.walkSpeed ? settings.acceleration : settings.runAcceleration;
            const float alongRate = isSpeedingUp ? speedUpRate : settings.deceleration;
            const float runBlend = math::clamp((speed - settings.walkSpeed)
                    / std::max(settings.runSpeed - settings.walkSpeed, k_minSpeed), 0.0f, 1.0f);
            const float sideRate = settings.acceleration
                + (settings.runTurnAcceleration - settings.acceleration) * runBlend;

            return along * approach(speed, wantedAlong, alongRate * delta)
                + approach(math::Vec3{0.0f, 0.0f, 0.0f}, wantedSide, sideRate * delta);
        }

        math::Vec3 easeAirVelocity(const math::Vec3& current, const math::Vec3& wanted, const PawnSettings& settings,
            float delta)
        {
            if (math::length(wanted) < k_minMoveLength)
            {
                return current * std::max(1.0f - settings.airDrag * delta, 0.0f);
            }

            return approach(current, wanted, settings.airAcceleration * delta);
        }

        void endSlide(PawnMotion& motion, const PawnSettings& settings)
        {
            motion.slideSeconds = 0.0f;
            motion.slideCooldown = settings.slideCooldown;
        }

        void updateSlideTimers(PawnMotion& motion, const PlayerCommand& command, const PawnSettings& settings,
            float delta)
        {
            const bool isCrouchPressed = command.crouch && !motion.isCrouchHeld;
            motion.isCrouchHeld = command.crouch;
            motion.slideCooldown = std::max(motion.slideCooldown - delta, 0.0f);

            if (isSliding(motion))
            {
                motion.slideSeconds = std::max(motion.slideSeconds - delta, 0.0f);

                if (!isSliding(motion))
                {
                    endSlide(motion, settings);
                }

                return;
            }

            const bool canSlide = isCrouchPressed && command.sprint && !command.aim && command.move.y > 0.0f
                && motion.isGrounded && motion.stance == PawnStance::Stand && motion.slideCooldown <= 0.0f
                && math::length(motion.planarVelocity) >= settings.slideMinSpeed && settings.slideSeconds > 0.0f;

            if (canSlide)
            {
                motion.slideSeconds = settings.slideSeconds;
            }
        }

        PawnStance wantedStance(const PawnMotion& motion, const PlayerCommand& command)
        {
            if (isSliding(motion))
            {
                return PawnStance::Slide;
            }

            return command.crouch ? PawnStance::Crouch : PawnStance::Stand;
        }

        void updateStance(PawnMotion& motion, physics::ControllerId controller, physics::PhysicsWorld& physics,
            const PlayerCommand& command, const PawnSettings& settings)
        {
            const PawnStance wanted = wantedStance(motion, command);

            if (wanted == motion.stance)
            {
                return;
            }

            const float height = stanceHeight(wanted, settings);
            const bool isShrinking = height <= stanceHeight(motion.stance, settings);

            if (isShrinking)
            {
                if (physics.setControllerHeight(controller, height, physics::ControllerResize::Force))
                {
                    motion.stance = wanted;
                }

                return;
            }

            if (physics.setControllerHeight(controller, height, physics::ControllerResize::MustFit))
            {
                motion.stance = wanted;
                return;
            }

            if (wanted == PawnStance::Stand && motion.stance == PawnStance::Slide
                && physics.setControllerHeight(controller, settings.crouchHeight, physics::ControllerResize::MustFit))
            {
                motion.stance = PawnStance::Crouch;
            }
        }

        void bufferJump(PawnMotion& motion, const PlayerCommand& command, const PawnSettings& settings, float delta)
        {
            const bool isJumpPressed = command.jump && !motion.isJumpHeld;
            motion.isJumpHeld = command.jump;

            if (isJumpPressed)
            {
                motion.jumpBufferTicks = secondsToTicks(settings.jumpBufferSeconds, delta);
            }
            else if (motion.jumpBufferTicks > 0)
            {
                --motion.jumpBufferTicks;
            }

            if (motion.isGrounded)
            {
                motion.coyoteTicks = secondsToTicks(settings.coyoteSeconds, delta);
            }
            else if (motion.coyoteTicks > 0)
            {
                --motion.coyoteTicks;
            }
        }

        bool tryJump(PawnMotion& motion, physics::ControllerId controller, physics::PhysicsWorld& physics,
            const PawnSettings& settings)
        {
            if (motion.jumpBufferTicks == 0 || (!motion.isGrounded && motion.coyoteTicks == 0))
            {
                return false;
            }

            if (motion.stance != PawnStance::Stand)
            {
                if (!physics.setControllerHeight(controller, k_pawnStandHeight, physics::ControllerResize::MustFit))
                {
                    return false;
                }

                motion.stance = PawnStance::Stand;
            }

            if (isSliding(motion))
            {
                endSlide(motion, settings);
            }

            motion.verticalSpeed = std::sqrt(2.0f * settings.gravity * std::max(settings.jumpHeight, 0.0f));
            motion.isGrounded = false;
            motion.jumpBufferTicks = 0;
            motion.coyoteTicks = 0;

            return true;
        }

        math::Vec3 rotateAboutUp(const math::Vec3& vector, float angle)
        {
            const float cosine = std::cos(angle);
            const float sine = std::sin(angle);

            return math::Vec3{vector.x * cosine + vector.z * sine, 0.0f, -vector.x * sine + vector.z * cosine};
        }

        math::Vec3 slideVelocity(const PawnMotion& motion, const PlayerCommand& command, const PawnSettings& settings,
            float delta)
        {
            const float currentSpeed = math::length(motion.planarVelocity);
            math::Vec3 direction = currentSpeed > k_minSpeed ? motion.planarVelocity * (1.0f / currentSpeed)
                                                             : forwardOf(command.yaw);
            const math::Vec3 wish = forwardOf(command.yaw) * command.move.y + rightOf(command.yaw) * command.move.x;

            if (math::length(wish) > k_minMoveLength)
            {
                const math::Vec3 wishDirection = math::normalize(wish);
                const float cross = direction.z * wishDirection.x - direction.x * wishDirection.z;
                const float angle = std::atan2(cross, math::dot(direction, wishDirection));
                const float maxTurn = settings.slideSteerRate * delta;
                direction = rotateAboutUp(direction, math::clamp(angle, -maxTurn, maxTurn));
            }

            const bool isFirstTick = motion.slideSeconds >= settings.slideSeconds;
            const float progress = motion.slideSeconds / std::max(settings.slideSeconds, k_minSpeed);
            const float burst = isFirstTick ? std::max(settings.slideSpeed, currentSpeed) : settings.slideSpeed;
            const float decayed = settings.slideEndSpeed + (burst - settings.slideEndSpeed) * progress * progress;
            const float speed = isFirstTick ? burst : std::min(decayed, currentSpeed);

            return direction * speed;
        }
        void updateTimers(PawnMotion& motion, const PlayerCommand& command, const PawnSettings& settings, float delta)
        {
            updateSlideTimers(motion, command, settings, delta);
            bufferJump(motion, command, settings, delta);
            updateKick(motion, command, settings, delta);

            if (motion.landingTicks > 0)
            {
                --motion.landingTicks;
            }

            if (isRunningCommand(command, motion) && math::length(motion.planarVelocity) > settings.walkSpeed)
            {
                motion.sprintRecoverTicks = secondsToTicks(settings.sprintRecoverSeconds, delta);
            }
            else if (motion.sprintRecoverTicks > 0)
            {
                --motion.sprintRecoverTicks;
            }
        }

        std::optional<physics::ControllerMove> moveFreely(PawnMotion& motion, physics::ControllerId controller,
            physics::PhysicsWorld& physics, const PlayerCommand& command, const PawnSettings& settings, float delta)
        {
            const bool wasGrounded = motion.isGrounded;

            if (!tryJump(motion, controller, physics, settings))
            {
                motion.verticalSpeed = motion.isGrounded ? -settings.groundStick
                                                         : motion.verticalSpeed - settings.gravity * delta;
            }

            const math::Vec3 wanted = wantedVelocity(command, motion, settings, delta);

            if (isSliding(motion))
            {
                motion.planarVelocity = slideVelocity(motion, command, settings, delta);
            }
            else if (motion.isGrounded)
            {
                motion.planarVelocity = easeGroundVelocity(motion.planarVelocity, wanted, settings, delta);
            }
            else
            {
                motion.planarVelocity = easeAirVelocity(motion.planarVelocity, wanted, settings, delta);
            }

            const math::Vec3 velocity = motion.planarVelocity + k_upAxis * motion.verticalSpeed;
            const std::optional<physics::ControllerMove> moved = physics.move(controller, velocity * delta, delta);

            if (!moved)
            {
                return std::nullopt;
            }

            motion.isGrounded = moved->grounded && motion.verticalSpeed <= 0.0f;

            if (motion.isGrounded && !wasGrounded)
            {
                landHard(motion, settings, -motion.verticalSpeed, delta);
            }

            if (moved->hitCeiling && motion.verticalSpeed > 0.0f)
            {
                motion.verticalSpeed = 0.0f;
            }

            return moved;
        }

        void stopBlockedSlide(PawnMotion& motion, const math::Vec3& start, const math::Vec3& end,
            const PawnSettings& settings, float delta)
        {
            math::Vec3 travelled = end - start;
            travelled.y = 0.0f;
            const float wanted = math::length(motion.planarVelocity) * delta;

            if (math::length(travelled) < wanted * k_slideBlockedFraction)
            {
                endSlide(motion, settings);
                motion.planarVelocity = travelled * (1.0f / std::max(delta, k_minSpeed));
            }
        }
    } //namespace

    std::optional<math::Vec3> stepPawn(PawnMotion& motion, physics::ControllerId controller,
        physics::PhysicsWorld& physics, const PlayerCommand& command, const PawnSettings& settings, float delta,
        std::span<const physics::BodyId> ignored)
    {
        const std::optional<math::Vec3> start = physics.footPosition(controller);

        if (!start)
        {
            return std::nullopt;
        }

        updateTimers(motion, command, settings, delta);
        updateStance(motion, controller, physics, command, settings);

        if (motion.stance != PawnStance::Slide && isSliding(motion))
        {
            endSlide(motion, settings);
        }

        if (!isMantling(motion) && !tryStartVault(motion, controller, physics, command, settings, delta, ignored))
        {
            tryStartMantle(motion, controller, physics, command, settings, delta, ignored);
        }

        std::optional<physics::ControllerMove> moved;

        if (isVaulting(motion))
        {
            moved = stepVault(motion, controller, physics, settings, delta);
        }
        else if (isMantling(motion))
        {
            moved = stepMantle(motion, controller, physics, settings, delta);
        }
        else
        {
            moved = moveFreely(motion, controller, physics, command, settings, delta);
        }

        if (!moved)
        {
            return std::nullopt;
        }

        updateLean(motion, controller, physics, command, settings, delta, ignored);
        const std::optional<math::Vec3> end = physics.footPosition(controller);

        if (end && isSliding(motion) && moved->hitSides)
        {
            stopBlockedSlide(motion, *start, *end, settings, delta);
        }

        return end;
    }

    float stanceHeight(PawnStance stance, const PawnSettings& settings)
    {
        switch (stance)
        {
            case PawnStance::Crouch:
                return settings.crouchHeight;
            case PawnStance::Slide:
                return settings.slideHeight;
            case PawnStance::Stand:
                break;
        }

        return k_pawnStandHeight;
    }

    void matchStanceHeight(physics::PhysicsWorld& physics, physics::ControllerId controller, const PawnMotion& motion,
        const PawnSettings& settings)
    {
        const float height = stanceHeight(motion.stance, settings);
        const std::optional<float> current = physics.controllerHeight(controller);

        if (current && std::abs(*current - height) > k_minMoveLength)
        {
            physics.setControllerHeight(controller, height, physics::ControllerResize::Force);
        }
    }

    bool isRunningCommand(const PlayerCommand& command, const PawnMotion& motion)
    {
        return command.sprint && !command.aim && motion.stance == PawnStance::Stand && !isSliding(motion)
            && command.move.y > 0.0f;
    }

    bool isWeaponReady(const PawnMotion& motion)
    {
        return motion.sprintRecoverTicks == 0;
    }

    bool isAimingReady(const PlayerCommand& command, const PawnMotion& motion)
    {
        return command.aim && isWeaponReady(motion);
    }

    uint8_t secondsToTicks(float seconds, float delta)
    {
        if (delta <= 0.0f || seconds <= 0.0f)
        {
            return 0;
        }

        return static_cast<uint8_t>(std::min(std::lround(seconds / delta), 255l));
    }

    bool isMantling(const PawnMotion& motion)
    {
        return motion.mantleTicks > 0;
    }

    bool isVaulting(const PawnMotion& motion)
    {
        return isMantling(motion) && motion.isVaulting;
    }

    bool isSliding(const PawnMotion& motion)
    {
        return motion.slideSeconds > 0.0f;
    }
} //namespace game
