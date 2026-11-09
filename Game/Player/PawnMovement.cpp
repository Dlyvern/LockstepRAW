#include "Player/PawnMovement.hpp"

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

        math::Vec3 wantedVelocity(const PlayerCommand& command, const PawnMotion& motion, const PawnSettings& settings)
        {
            math::Vec2 move = command.move;
            const float moveLength = math::length(move);

            if (moveLength < k_minMoveLength)
            {
                return math::Vec3{0.0f, 0.0f, 0.0f};
            }

            const bool isCrouched = motion.stance != PawnStance::Stand;
            const bool isRunning = command.sprint && !command.aim && !isCrouched && move.y > 0.0f;
            float speedCap = command.aim ? settings.aimSpeed : std::numeric_limits<float>::max();

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
            const float speed = std::min(moveLength, 1.0f) * ellipseSpeed(direction, sideSpeed, forwardSpeed);

            return (forwardOf(command.yaw) * direction.y + rightOf(command.yaw) * direction.x) * speed;
        }

        math::Vec3 easeVelocity(const math::Vec3& current, const math::Vec3& wanted, const PawnSettings& settings,
            float delta)
        {
            const math::Vec3 difference = wanted - current;
            const float distance = math::length(difference);
            const bool isSpeedingUp = math::length(wanted) > math::length(current);
            const float maxChange = (isSpeedingUp ? settings.acceleration : settings.deceleration) * delta;

            if (distance <= maxChange)
            {
                return wanted;
            }

            return current + difference * (maxChange / distance);
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
    } //namespace

    std::optional<math::Vec3> stepPawn(PawnMotion& motion, physics::ControllerId controller,
        physics::PhysicsWorld& physics, const PlayerCommand& command, const PawnSettings& settings, float delta)
    {
        const std::optional<math::Vec3> start = physics.footPosition(controller);

        if (!start)
        {
            return std::nullopt;
        }

        updateSlideTimers(motion, command, settings, delta);
        updateStance(motion, controller, physics, command, settings);

        if (motion.stance != PawnStance::Slide && isSliding(motion))
        {
            endSlide(motion, settings);
        }

        motion.verticalSpeed = motion.isGrounded ? -settings.groundStick : motion.verticalSpeed - settings.gravity * delta;
        motion.planarVelocity = isSliding(motion)
            ? slideVelocity(motion, command, settings, delta)
            : easeVelocity(motion.planarVelocity, wantedVelocity(command, motion, settings), settings, delta);

        const math::Vec3 velocity = motion.planarVelocity + k_upAxis * motion.verticalSpeed;
        const std::optional<physics::ControllerMove> moved = physics.move(controller, velocity * delta, delta);

        if (!moved)
        {
            return std::nullopt;
        }

        motion.isGrounded = moved->grounded;

        if (moved->hitCeiling && motion.verticalSpeed > 0.0f)
        {
            motion.verticalSpeed = 0.0f;
        }

        const std::optional<math::Vec3> end = physics.footPosition(controller);

        if (end && isSliding(motion) && moved->hitSides)
        {
            math::Vec3 travelled = *end - *start;
            travelled.y = 0.0f;
            const float wanted = math::length(motion.planarVelocity) * delta;

            if (math::length(travelled) < wanted * k_slideBlockedFraction)
            {
                endSlide(motion, settings);
                motion.planarVelocity = travelled * (1.0f / std::max(delta, k_minSpeed));
            }
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

    bool isSliding(const PawnMotion& motion)
    {
        return motion.slideSeconds > 0.0f;
    }
} //namespace game
