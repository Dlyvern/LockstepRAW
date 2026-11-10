#ifndef LOCKSTEP_GAME_PLAYER_PAWN_MOVEMENT_HPP
#define LOCKSTEP_GAME_PLAYER_PAWN_MOVEMENT_HPP

#include "Player/KickSettings.hpp"
#include "Player/PlayerCommand.hpp"

#include <Lockstep/Core/Math.hpp>
#include <Lockstep/Physics/PhysicsTypes.hpp>

#include <cstdint>
#include <optional>
#include <span>

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
    //                 command asks for by the PawnSettings acceleration curves (on the ground) or
    //                 air control (airborne); while sliding it is the slide velocity.
    // stance:         the capsule size.
    // isCrouchHeld:   the previous command asked to crouch (a rising edge can start a slide).
    // slideSeconds:   Player seconds of slide left; 0 while not sliding.
    // slideCooldown:  Player seconds until the next slide may start.
    // isJumpHeld:     the previous command held jump (a rising edge buffers a jump).
    // jumpBufferTicks: ticks a jump press stays buffered (jump buffering).
    // coyoteTicks:    ticks a jump is still allowed after walking off an edge (coyote time).
    // landingTicks:   ticks of slowdown left after a hard landing.
    // lean:           -1 (full lean left) to 1 (full right); the eyes move lean * leanDistance
    //                 sideways. Shortened where a wall is in the way.
    // sprintRecoverTicks: ticks until the weapon can fire or aim again after running.
    // mantleTicks:    while mantling, the ticks of its forward part left (it first rises to
    //                 mantleHeight); 0 while not mantling.
    // mantleHeight:   the foot height a mantle rises to, metres (world y).
    // isVaulting:     the mantle is a vault: it rises to mantleHeight (just above the obstacle),
    //                 crosses it at planarVelocity for mantleTicks and then drops on the far side.
    // isKickHeld:     the previous command held kick (a rising edge kicks).
    // kickTicks:      ticks since the kick started, counting from 1; 0 while not kicking.
    // kickCooldownTicks: ticks until the next kick may start.
    struct PawnMotion
    {
        float verticalSpeed{0.0f};
        bool isGrounded{false};
        math::Vec3 planarVelocity{0.0f, 0.0f, 0.0f};
        PawnStance stance{PawnStance::Stand};
        bool isCrouchHeld{false};
        float slideSeconds{0.0f};
        float slideCooldown{0.0f};
        bool isJumpHeld{false};
        uint8_t jumpBufferTicks{0};
        uint8_t coyoteTicks{0};
        uint8_t landingTicks{0};
        float lean{0.0f};
        uint8_t sprintRecoverTicks{0};
        uint8_t mantleTicks{0};
        float mantleHeight{0.0f};
        bool isVaulting{false};
        bool isKickHeld{false};
        uint8_t kickTicks{0};
        uint8_t kickCooldownTicks{0};

        friend bool operator==(const PawnMotion&, const PawnMotion&) = default;
    };

    // How a pawn vaults over a thin waist-high obstacle (a road barrier, a railing, a low wall):
    // a jump press while moving forward close to it goes over in one motion instead of jumping.
    // minHeight, maxHeight: obstacle top heights above the feet that can be vaulted, metres.
    // reach:        how far in front of the capsule the obstacle is found, metres, plus
    //               reachPerSpeed for each m/s of planar speed.
    // reachPerSpeed: see reach.
    // maxThickness: thickest obstacle vaulted, metres; a thicker one is mantled onto instead.
    // clearance:    how far the feet pass above the top, metres.
    // landMargin:   room kept between the capsule and the far face when landing, metres.
    // sprintDistance: extra landing distance per m/s of speed above walking, metres (a sprint
    //               vault goes further).
    // maxDrop:      deepest floor below the feet on the far side, metres.
    // riseSpeed:    upward speed of the hop onto the top, m/s, plus riseSpeedPerSpeed for each
    //               m/s of speed above walking. The pawn keeps moving forward while it rises
    //               (up to the obstacle's face).
    // riseSpeedPerSpeed: see riseSpeed.
    // minSpeed, maxSpeed: the speed the obstacle is crossed at: the entry speed, clamped.
    // exitScale:    planar speed kept when the pawn drops off the far side, as a fraction.
    // exitDropSpeed: downward speed the drop starts with, m/s.
    struct VaultSettings
    {
        float minHeight{0.45f};
        float maxHeight{1.3f};
        float reach{0.55f};
        float reachPerSpeed{0.07f};
        float maxThickness{0.9f};
        float clearance{0.08f};
        float landMargin{0.12f};
        float sprintDistance{0.05f};
        float maxDrop{1.5f};
        float riseSpeed{6.5f};
        float riseSpeedPerSpeed{0.6f};
        float minSpeed{4.2f};
        float maxSpeed{7.5f};
        float exitScale{0.85f};
        float exitDropSpeed{1.5f};
    };

    // Pawn movement and health settings, the simulation tunables of movement. Speeds are metres
    // per second of Player time; a diagonal move gets the speed of the ellipse through the
    // forward/backward and strafe speeds. Host and client must use the same values (the client
    // predicts with its own copy).
    // walkSpeed:    moving forward without sprint (a brisk combat jog, not a stroll).
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
    // acceleration: m/s^2 while speeding up on the ground below walk speed (about 0.1 s from
    //               standing to the default move speed).
    // runAcceleration: m/s^2 while speeding up on the ground above walk speed (into a run).
    // deceleration: m/s^2 while slowing down on the ground (a little slower than starting: about
    //               0.15 s from moving, 0.25 s from running, so stops are short but not instant).
    // runTurnAcceleration: m/s^2 of sideways change at run speed, so turning while running is
    //               slower than while walking (which turns at `acceleration`).
    // airAcceleration: m/s^2 of steering while airborne (limited air control).
    // airDrag:      fraction of planar speed lost per second while airborne without input.
    // jumpHeight:   how high a standing jump lifts the feet, metres.
    // coyoteSeconds: how long after leaving the ground a jump still works.
    // jumpBufferSeconds: how long a jump press before landing is remembered.
    // hardLandingSpeed: fall speed (m/s) from which a landing slows the pawn (a plain jump lands
    //               at about 4.7 m/s; 8.5 is walking off a drop of about 1.3 m).
    // hardLandingFullSpeed: fall speed of the longest slowdown.
    // hardLandingMinSeconds, hardLandingMaxSeconds: slowdown length at those two speeds.
    // hardLandingScale: speed multiplier right after a hard landing; recovers to 1 over the
    //               last hardLandingMinSeconds of the slowdown.
    // leanDistance: how far a full lean moves the eyes sideways, metres.
    // leanSeconds:  time from upright to a full lean.
    // leanClearance: room kept between the leaned eyes and a wall, metres.
    // sprintRecoverSeconds: after running stops, how long the weapon can't fire or aim.
    // mantleMinHeight, mantleMaxHeight: ledge heights above the feet that can be mantled, metres
    //               (lower ones are climbed as steps).
    // mantleReach:  how far in front of the capsule a ledge is found, metres.
    // mantleRiseSpeed: upward speed while mantling, m/s.
    // mantleForwardDistance: how far the pawn moves onto the ledge after rising, metres.
    // mantleForwardSeconds: how long that takes.
    // vault:        vaulting over thin obstacles (tried before a mantle).
    // kick:         the front kick.
    // gravity:      downward acceleration while airborne, in m/s^2.
    // groundStick:  downward speed kept while grounded, so the pawn follows steps and ramps down.
    // maxHealth:    hit points after (re)spawning.
    // respawnDelay: Player seconds from death to respawn.
    struct PawnSettings
    {
        float walkSpeed{3.8f};
        float runSpeed{6.4f};
        float weaponUpRunSpeed{6.0f};
        float backSpeed{3.0f};
        float strafeSpeed{3.4f};
        float sprintStrafeSpeed{4.2f};
        float aimSpeed{2.4f};
        float crouchSpeed{2.2f};
        float crouchHeight{1.2f};
        float slideHeight{1.0f};
        float slideSpeed{8.5f};
        float slideEndSpeed{2.6f};
        float slideSeconds{0.75f};
        float slideCooldown{0.6f};
        float slideMinSpeed{4.6f};
        float slideSteerRate{0.0f};
        float acceleration{40.0f};
        float runAcceleration{14.0f};
        float deceleration{26.0f};
        float runTurnAcceleration{20.0f};
        float airAcceleration{4.0f};
        float airDrag{0.2f};
        float jumpHeight{0.55f};
        float coyoteSeconds{0.1f};
        float jumpBufferSeconds{0.1f};
        float hardLandingSpeed{8.5f};
        float hardLandingFullSpeed{14.0f};
        float hardLandingMinSeconds{0.2f};
        float hardLandingMaxSeconds{0.6f};
        float hardLandingScale{0.4f};
        float leanDistance{0.35f};
        float leanSeconds{0.2f};
        float leanClearance{0.2f};
        float sprintRecoverSeconds{0.15f};
        float mantleMinHeight{0.4f};
        float mantleMaxHeight{1.3f};
        float mantleReach{0.6f};
        float mantleRiseSpeed{5.5f};
        float mantleForwardDistance{0.7f};
        float mantleForwardSeconds{0.12f};
        VaultSettings vault{};
        KickSettings kick{};
        float gravity{20.0f};
        float groundStick{4.5f};
        int32_t maxHealth{100};
        float respawnDelay{3.0f};
    };

    // Moves a pawn's controller by one command for one tick: walks, runs, crouches (shrinking the
    // capsule; standing up only where it fits), slides (a sprint plus a fresh crouch press starts
    // a speed burst that decays over PawnSettings::slideSeconds with limited steering, then
    // continues as a crouch), jumps (a buffered press, with coyote time; from a crouch or a slide
    // only where standing up fits; a slide keeps its speed into the jump), mantles or vaults
    // instead when the press meets a thin obstacle (PawnVault.hpp) or a ledge (PawnMantle.hpp) in
    // front while moving forward, falls (a hard landing
    // slows it for a moment), climbs steps and ramps, and leans (updateLean()). The one movement
    // rule, used by the host for every pawn and by the client to predict and replay its own.
    // motion:     the pawn's velocity, grounding, stance and slide timers, updated.
    // controller: the pawn's controller.
    // physics:    moves and resizes the controller.
    // command:    the command of this tick.
    // settings:   movement settings.
    // delta:      the Player domain's delta for this tick, in seconds.
    // ignored:    bodies the pawn's rays pass through (the host's dynamic toys, which a client
    //             does not have).
    // Returns:    the new foot position, or nothing for a stale controller.
    std::optional<math::Vec3> stepPawn(PawnMotion& motion, lockstep::physics::ControllerId controller,
        lockstep::physics::PhysicsWorld& physics, const PlayerCommand& command, const PawnSettings& settings,
        float delta, std::span<const lockstep::physics::BodyId> ignored = {});

    // motion:  a pawn's motion.
    // Returns: true when the weapon may fire and aim (not running and not just stopped running).
    [[nodiscard]] bool isWeaponReady(const PawnMotion& motion);

    // command: a pawn's command.
    // motion:  a pawn's motion.
    // Returns: true when the command aims down sights and the weapon is ready (isWeaponReady()).
    [[nodiscard]] bool isAimingReady(const PlayerCommand& command, const PawnMotion& motion);

    // command: a pawn's command.
    // motion:  its motion.
    // Returns: true while the command runs: sprint held, moving forward, standing, not aiming or
    //          sliding.
    [[nodiscard]] bool isRunningCommand(const PlayerCommand& command, const PawnMotion& motion);

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

    // seconds: a duration.
    // delta:   the Player domain's delta for one tick.
    // Returns: the duration in whole ticks (at least 0, at most 255).
    [[nodiscard]] uint8_t secondsToTicks(float seconds, float delta);

    // motion:  a pawn's motion.
    // Returns: true while it mantles or vaults.
    [[nodiscard]] bool isMantling(const PawnMotion& motion);

    // motion:  a pawn's motion.
    // Returns: true while it vaults (a kind of mantle).
    [[nodiscard]] bool isVaulting(const PawnMotion& motion);

    // motion:  a pawn's motion.
    // Returns: true while it slides.
    [[nodiscard]] bool isSliding(const PawnMotion& motion);
} //namespace game

#endif //LOCKSTEP_GAME_PLAYER_PAWN_MOVEMENT_HPP
