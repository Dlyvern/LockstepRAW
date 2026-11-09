#ifndef LOCKSTEP_GAME_PLAYER_PAWNS_HPP
#define LOCKSTEP_GAME_PLAYER_PAWNS_HPP

#include "Player/PlayerCommand.hpp"

#include <Lockstep/Core/Math.hpp>
#include <Lockstep/Core/Result.hpp>
#include <Lockstep/Core/Transform.hpp>
#include <Lockstep/Physics/PhysicsTypes.hpp>
#include <Lockstep/World/Entities.hpp>

#include <cstddef>
#include <cstdint>
#include <optional>

namespace lockstep::net
{
    class Replication;
} //namespace lockstep::net

namespace lockstep::physics
{
    class PhysicsWorld;
} //namespace lockstep::physics

namespace game
{
    namespace math = lockstep::math;

    // Players in a game: Player1 (the host) and Player2 (the client).
    inline constexpr std::size_t k_playerCount{2};

    // Half the size of a pawn's body (its capsule's bounds), in metres.
    inline constexpr math::Vec3 k_pawnHalfExtents{0.3f, 0.9f, 0.3f};

    // Height of the eyes above the pawn's centre, in metres.
    inline constexpr float k_pawnEyeOffset{0.7f};

    // The movement state a pawn carries from tick to tick besides its controller's position.
    // Host and client step it with the same code (stepPawn()), so the client can predict it.
    // verticalSpeed:  upward speed in m/s (negative while falling).
    // isGrounded:     whether the last move ended on the ground.
    // planarVelocity: wanted horizontal velocity in m/s (y is always 0), eased towards what the
    //                 command asks for by PawnSettings::acceleration / deceleration.
    struct PawnMotion
    {
        float verticalSpeed{0.0f};
        bool isGrounded{false};
        math::Vec3 planarVelocity{0.0f, 0.0f, 0.0f};

        friend bool operator==(const PawnMotion&, const PawnMotion&) = default;
    };

    // One player's pawn on the host: its entity and controller plus the input, health and
    // counters that drive it.
    // entity:         the pawn entity (Player time domain); null while absent or dead.
    // controller:     the pawn's capsule controller; null while absent or dead.
    // command:        the newest command of the player.
    // hasCommand:     false until the first command arrived after the player joined.
    // processedShots: the command shot counter the host already fired for.
    // processedInteracts: the command interact counter the host already handled.
    // motion:         velocity and grounding.
    // life:           counts spawns (wrapping), so the client can tell a respawn from a move.
    // isJoined:       the player is in the game (alive or waiting to respawn).
    // health:         hit points left; 0 while dead.
    // respawnTimer:   Player seconds left until it respawns, while dead.
    // isArmed:        the player carries the weapon (host-authoritative, kept across respawns).
    // isLowered:      the armed player's weapon is lowered (lower toggle and weapon zone, decided
    //                 by the host every tick).
    // shots, hits:    shots fired and shots that hit the enemy, for the dev panel.
    // deaths:         times it died, for the dev panel.
    struct Pawn
    {
        lockstep::world::EntityId entity{};
        lockstep::physics::ControllerId controller{};
        PlayerCommand command{};
        bool hasCommand{false};
        uint8_t processedShots{0};
        uint8_t processedInteracts{0};
        PawnMotion motion{};
        uint8_t life{0};
        bool isJoined{false};
        int32_t health{0};
        float respawnTimer{0.0f};
        bool isArmed{false};
        bool isLowered{false};
        uint32_t shots{0};
        uint32_t hits{0};
        uint32_t deaths{0};
    };

    // Pawn movement and health settings. Speeds are metres per second of Player time; a diagonal
    // move gets the speed of the ellipse through the forward/backward and strafe speeds.
    // walkSpeed:    walking forward.
    // runSpeed:     running forward while sprint is held (forward and forward-diagonal moves only).
    // weaponUpRunSpeed: cap on running while the weapon is raised (PlayerCommand::isWeaponUp).
    // backSpeed:    walking backward.
    // strafeSpeed:  walking sideways.
    // sprintStrafeSpeed: sideways part of a forward-diagonal run.
    // aimSpeed:     cap on every direction while aiming (no running then).
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
        float acceleration{6.0f};
        float deceleration{10.0f};
        float gravity{18.0f};
        float groundStick{3.0f};
        int32_t maxHealth{100};
        float respawnDelay{3.0f};
    };

    // Settings of a pawn's capsule controller.
    // footPosition: where its feet start.
    // Returns:      the description, the same on host and client.
    [[nodiscard]] lockstep::physics::ControllerDescription pawnController(const math::Vec3& footPosition);

    // Host: brings a pawn alive: creates its entity in the Player time domain, tags it for
    // replication and creates its capsule controller. Resets health, movement and the respawn
    // timer, advances its life; keeps the command and the counters.
    // pawn:        the pawn; must have no entity.
    // entities:    the entity store.
    // replication: tags the entity as Archetype::Pawn.
    // physics:     creates the controller.
    // owner:       the player who controls it (Player1 is the host, Player2 the client).
    // footPosition: where its feet start.
    // settings:    health settings.
    // Returns:     success, or an error if the entity or the controller could not be created
    //              (nothing is left behind then).
    [[nodiscard]] lockstep::Status spawnPawn(Pawn& pawn, lockstep::world::Entities& entities,
        lockstep::net::Replication& replication, lockstep::physics::PhysicsWorld& physics,
        lockstep::world::EntityOwner owner, const math::Vec3& footPosition, const PawnSettings& settings);

    // Host: removes a pawn's entity and controller. Health and timers are left to the caller.
    // pawn:     the pawn.
    // entities: the entity store.
    // physics:  the physics world.
    void despawnPawn(Pawn& pawn, lockstep::world::Entities& entities, lockstep::physics::PhysicsWorld& physics);

    // Moves a pawn's controller by one command for one tick: walks or runs (easing its speed),
    // falls, climbs steps and ramps. The one movement rule, used by the host for every pawn and by the client to predict
    // and replay its own.
    // motion:     the pawn's velocity and grounding, updated.
    // controller: the pawn's controller.
    // physics:    moves the controller.
    // command:    the command of this tick.
    // settings:   movement settings.
    // delta:      the Player domain's delta for this tick, in seconds.
    // Returns:    the new foot position, or nothing for a stale controller.
    std::optional<math::Vec3> stepPawn(PawnMotion& motion, lockstep::physics::ControllerId controller,
        lockstep::physics::PhysicsWorld& physics, const PlayerCommand& command, const PawnSettings& settings,
        float delta);

    // Places a pawn entity: centre above `footPosition`, turned to `yaw`, pawn-sized.
    // entity:       the entity.
    // footPosition: the controller's foot position.
    // yaw:          look angle around the up axis.
    void placePawnEntity(lockstep::world::Entity& entity, const math::Vec3& footPosition, float yaw);

    // Host: moves and turns a living pawn by its command for one tick with stepPawn(), then
    // copies the result into its entity.
    // pawn:     the pawn.
    // entity:   the pawn's entity.
    // physics:  moves the controller.
    // settings: movement settings.
    // delta:    the Player domain's delta for this tick, in seconds.
    void movePawn(Pawn& pawn, lockstep::world::Entity& entity, lockstep::physics::PhysicsWorld& physics,
        const PawnSettings& settings, float delta);

    // transform: a pawn entity's transform.
    // Returns:   the foot position of its controller.
    [[nodiscard]] math::Vec3 pawnFootPosition(const math::Transform& transform);

    // transform: a pawn's transform.
    // Returns:   the world position of the pawn's eyes.
    [[nodiscard]] math::Vec3 eyePosition(const math::Transform& transform);

    // yaw:     look angle around the up axis, 0 looks along -z.
    // pitch:   look angle up (positive) or down.
    // Returns: the unit look direction.
    [[nodiscard]] math::Vec3 lookDirection(float yaw, float pitch);
} //namespace game

#endif //LOCKSTEP_GAME_PLAYER_PAWNS_HPP
