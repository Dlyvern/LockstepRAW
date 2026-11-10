#ifndef LOCKSTEP_GAME_PLAYER_PAWNS_HPP
#define LOCKSTEP_GAME_PLAYER_PAWNS_HPP

#include "Player/PawnMovement.hpp"
#include "Player/PlayerCommand.hpp"
#include "Weapons/Ammo.hpp"

#include <Lockstep/Core/Math.hpp>
#include <Lockstep/Core/Result.hpp>
#include <Lockstep/Core/Transform.hpp>
#include <Lockstep/Physics/PhysicsTypes.hpp>
#include <Lockstep/World/Entities.hpp>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

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

    // One player's pawn on the host: its entity and controller plus the input, health and
    // counters that drive it.
    // entity:         the pawn entity (Player time domain); null while absent or dead.
    // controller:     the pawn's capsule controller; null while absent or dead.
    // command:        the newest command of the player.
    // hasCommand:     false until the first command arrived after the player joined.
    // processedShots: the command shot counter the host already fired for.
    // processedInteracts: the command interact counter the host already handled.
    // motion:         velocity, grounding, stance and slide timers.
    // life:           counts spawns (wrapping), so the client can tell a respawn from a move.
    // isJoined:       the player is in the game (alive or waiting to respawn).
    // health:         hit points left; 0 while dead.
    // respawnTimer:   Player seconds left until it respawns, while dead.
    // isArmed:        the player carries the weapon (host-authoritative, kept across respawns).
    // isLowered:      the armed player's weapon is lowered (lower toggle and weapon zone, decided
    //                 by the host every tick).
    // shots, hits:    shots fired and shots that hit the enemy, for the dev panel.
    // deaths:         times it died, for the dev panel.
    // isDowned:       health ran out: the player crawls, can't shoot and bleeds out unless the
    //                 partner revives them (PawnHealth.hpp).
    // bleedSeconds:   Player seconds left before a downed player bleeds out.
    // reviveSeconds:  how long the partner has held F next to this downed player.
    // damageCount:    hits taken, wrapping; each increase flashes the HUD.
    // damageYaw:      world yaw towards the newest hit's source (quantizeYaw()).
    // hitCount:       hits this player landed on hostile NPCs, wrapping (hit markers).
    // killCount:      hostile NPCs this player killed, wrapping (kill markers).
    // ammo:           magazine, reserve and reload.
    // shotCooldown:   Player seconds until the next shot may fire (the cadence check).
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
        bool isDowned{false};
        float bleedSeconds{0.0f};
        float reviveSeconds{0.0f};
        uint8_t damageCount{0};
        uint8_t damageYaw{0};
        uint8_t hitCount{0};
        uint8_t killCount{0};
        Ammo ammo{};
        float shotCooldown{0.0f};
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

    // Places a pawn entity: centre above `footPosition`, turned to `yaw`, sized like its capsule
    // (the entity scale's y is the capsule height, so the replicated transform carries crouching),
    // and rolled about its feet so its eyes sit `leanOffset` to the side (the replicated rotation
    // carries the lean; eyePosition() and the partner's drawn body follow it).
    // entity:       the entity.
    // footPosition: the controller's foot position.
    // yaw:          look angle around the up axis.
    // height:       the capsule height (stanceHeight()).
    // leanOffset:   sideways eye offset of the lean, metres (leanOffset() in PawnLean.hpp).
    void placePawnEntity(lockstep::world::Entity& entity, const math::Vec3& footPosition, float yaw, float height,
        float leanOffset);

    // Host: moves and turns a living pawn by its command for one tick with stepPawn(), then
    // copies the result into its entity.
    // pawn:     the pawn.
    // entity:   the pawn's entity.
    // physics:  moves the controller.
    // settings: movement settings.
    // delta:    the Player domain's delta for this tick, in seconds.
    // ignored:  bodies the pawn's rays pass through (the host's dynamic toys).
    void movePawn(Pawn& pawn, lockstep::world::Entity& entity, lockstep::physics::PhysicsWorld& physics,
        const PawnSettings& settings, float delta, std::span<const lockstep::physics::BodyId> ignored = {});

    // transform: a pawn entity's transform.
    // Returns:   the foot position of its controller.
    [[nodiscard]] math::Vec3 pawnFootPosition(const math::Transform& transform);

    // transform: a pawn's transform.
    // Returns:   the world position of the pawn's eyes, k_pawnEyeBelowTop under the top of its
    //            capsule (lower while crouched or sliding), moved sideways by its lean.
    [[nodiscard]] math::Vec3 eyePosition(const math::Transform& transform);

    // transform: a pawn's transform.
    // Returns:   its lean roll about the feet, radians (positive leans right).
    [[nodiscard]] float pawnLeanRoll(const math::Transform& transform);

    // transform: a pawn entity's (interpolated) transform.
    // settings:  movement settings (stand and crouch heights).
    // Returns:   how far it is crouched: 0 standing, 1 at crouch height, above 1 lower (sliding).
    [[nodiscard]] float pawnCrouchAmount(const math::Transform& transform, const PawnSettings& settings);

    // yaw:     look angle around the up axis, 0 looks along -z.
    // pitch:   look angle up (positive) or down.
    // Returns: the unit look direction.
    [[nodiscard]] math::Vec3 lookDirection(float yaw, float pitch);
} //namespace game

#endif //LOCKSTEP_GAME_PLAYER_PAWNS_HPP
