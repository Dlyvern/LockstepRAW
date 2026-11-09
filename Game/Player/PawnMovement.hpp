#ifndef LOCKSTEP_GAME_PLAYER_PAWN_MOVEMENT_HPP
#define LOCKSTEP_GAME_PLAYER_PAWN_MOVEMENT_HPP

#include "Player/PlayerCommand.hpp"

#include <Lockstep/Core/Math.hpp>
#include <Lockstep/Physics/PhysicsTypes.hpp>

#include <cstdint>
#include <optional>

namespace lockstep::physics
{
    class PhysicsWorld;
} //namespace lockstep::physics

namespace game
{
    namespace math = lockstep::math;

    // Radius of a pawn's capsule, in metres.
    inline constexpr float k_pawnRadius{0.3f};

    // Height of a standing pawn's capsule (feet to top of the head), in metres.
    inline constexpr float k_pawnStandHeight{1.8f};

    // How far the eyes sit below the top of the capsule, in metres (standing, crouched or sliding).
    inline constexpr float k_pawnEyeBelowTop{0.2f};

    // The size a pawn's capsule has. A pawn only grows back (Slide -> Crouch -> Stand) when the
    // taller capsule fits, so it stays low under a ceiling.
    // Stand:  full height.
    // Crouch: PawnSettings::crouchHeight.
    // Slide:  PawnSettings::slideHeight.
    enum class PawnStance : uint8_t
    {
        Stand = 0,
        Crouch,
        Slide
    };

    // The movement state a pawn carries from tick to tick besides its controller's position.
    // Host and client step it with the same code (stepPawn()), so the client can predict it.
    // verticalSpeed:  upward speed in m/s (negative while falling).
    // isGrounded:     whether the last move ended on the ground.
    // planarVelocity: wanted horizontal velocity in m/s (y is always 0), eased towards what the
    //                 command asks for by PawnSettings::acceleration / deceleration; while sliding
    //                 it is the slide velocity.
    // stance:         the capsule size.
    // isCrouchHeld:   the previous command asked to crouch (a rising edge can start a slide).
    // slideSeconds:   Player seconds of slide left; 0 while not sliding.
    // slideCooldown:  Player seconds until the next slide may start.
    struct PawnMotion
    {
        float verticalSpeed{0.0f};
        bool isGrounded{false};
        math::Vec3 planarVelocity{0.0f, 0.0f, 0.0f};
        PawnStance stance{PawnStance::Stand};
        bool isCrouchHeld{false};
        float slideSeconds{0.0f};
        float slideCooldown{0.0f};

        friend bool operator==(const PawnMotion&, const PawnMotion&) = default;
    };

    // Pawn movement and health settings, the simulation tunables of movement. Speeds are metres
    // per second of Player time; a diagonal move gets the speed of the ellipse through the
    // forward/backward and strafe speeds. Host and client must use the same values (the client
    // predicts with its own copy).
    // walkSpeed:    walking forward.
    // runSpeed:     running forward while sprint is held (forward and forward-diagonal moves only).
    // weaponUpRunSpeed: cap on running while the weapon is raised (PlayerCommand::isWeaponUp).
    // backSpeed:    walking backward.
    // strafeSpeed:  walking sideways.
    // sprintStrafeSpeed: sideways part of a forward-diagonal run.
    // aimSpeed:     cap on every direction while aiming (no running then).
    // crouchSpeed:  cap on every direction while crouched (no running then).
    // crouchHeight: capsule height while crouched, metres (more than 2 * k_pawnRadius).
    // slideHeight:  capsule height while sliding, metres (more than 2 * k_pawnRadius).
    // slideSpeed:   speed at the start of a slide (or the run speed, if faster).
    // slideEndSpeed: speed the slide has decayed to when it ends.
    // slideSeconds: how long a slide lasts.
    // slideCooldown: Player seconds after a slide before the next may start.
    // slideMinSpeed: a slide only starts from at least this planar speed.
    // slideSteerRate: how fast a slide turns towards the move direction, rad/s.
    // acceleration: m/s^2 while speeding up towards the wanted velocity.
    // deceleration: m/s^2 while slowing down.
    // gravity:      downward acceleration while airborne, in m/s^2.
    // groundStick:  downward speed kept while grounded, so the pawn follows steps and ramps down.
    // maxHealth:    hit points after (re)spawning.
    // respawnDelay: Player seconds from death to respawn.
    struct PawnSettings
    {
        float walkSpeed{1.6f};
        float runSpeed{4.0f};
        float weaponUpRunSpeed{2.8f};
        float backSpeed{1.2f};
        float strafeSpeed{1.6f};
        float sprintStrafeSpeed{1.6f};
        float aimSpeed{1.3f};
        float crouchSpeed{1.0f};
        float crouchHeight{1.2f};
        float slideHeight{1.0f};
        float slideSpeed{5.5f};
        float slideEndSpeed{1.6f};
        float slideSeconds{0.8f};
        float slideCooldown{0.6f};
        float slideMinSpeed{2.4f};
        float slideSteerRate{1.0f};
        float acceleration{6.0f};
        float deceleration{10.0f};
        float gravity{18.0f};
        float groundStick{3.0f};
        int32_t maxHealth{100};
        float respawnDelay{3.0f};
    };

    // Moves a pawn's controller by one command for one tick: walks, runs, crouches (shrinking the
    // capsule; standing up only where it fits), slides (a sprint plus a fresh crouch press starts
    // a speed burst that decays over PawnSettings::slideSeconds with limited steering, then
    // continues as a crouch), falls, climbs steps and ramps. The one movement rule, used by the
    // host for every pawn and by the client to predict and replay its own.
    // motion:     the pawn's velocity, grounding, stance and slide timers, updated.
    // controller: the pawn's controller.
    // physics:    moves and resizes the controller.
    // command:    the command of this tick.
    // settings:   movement settings.
    // delta:      the Player domain's delta for this tick, in seconds.
    // Returns:    the new foot position, or nothing for a stale controller.
    std::optional<math::Vec3> stepPawn(PawnMotion& motion, lockstep::physics::ControllerId controller,
        lockstep::physics::PhysicsWorld& physics, const PlayerCommand& command, const PawnSettings& settings,
        float delta);

    // stance:   a capsule size.
    // settings: movement settings.
    // Returns:  the capsule height of that stance, in metres.
    [[nodiscard]] float stanceHeight(PawnStance stance, const PawnSettings& settings);

    // Gives a controller the capsule height of a motion's stance without a fit check, e.g. after
    // the client takes over the host's state of its pawn.
    // physics:    the physics world.
    // controller: the controller.
    // motion:     the motion whose stance to match.
    // settings:   movement settings.
    void matchStanceHeight(lockstep::physics::PhysicsWorld& physics, lockstep::physics::ControllerId controller,
        const PawnMotion& motion, const PawnSettings& settings);

    // motion:  a pawn's motion.
    // Returns: true while it slides.
    [[nodiscard]] bool isSliding(const PawnMotion& motion);
} //namespace game

#endif //LOCKSTEP_GAME_PLAYER_PAWN_MOVEMENT_HPP
