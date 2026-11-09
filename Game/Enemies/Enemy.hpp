#ifndef LOCKSTEP_GAME_ENEMIES_ENEMY_HPP
#define LOCKSTEP_GAME_ENEMIES_ENEMY_HPP

#include <Lockstep/Core/Math.hpp>
#include <Lockstep/Core/Result.hpp>
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

    // Half the size of the enemy's body (its capsule's bounds), in metres.
    inline constexpr math::Vec3 k_enemyHalfExtents{0.45f, 1.0f, 0.45f};

    // Alive: the enemy exists and hunts the players. Dead: it is gone until it respawns.
    enum class EnemyPhase : uint8_t
    {
        Alive,
        Dead
    };

    // The host's enemy: a World-domain entity with its own controller that walks toward the
    // nearest player and strikes at melee range, so bullet-time slows both.
    // entity:         the enemy entity; null while dead.
    // controller:     its capsule controller; null while dead.
    // phase:          alive or dead.
    // health:         hits left before it dies.
    // respawnTimer:   World seconds left until it respawns, while dead.
    // attackCooldown: World seconds left until it may strike again.
    // verticalSpeed:  upward speed in m/s (negative while falling).
    // isGrounded:     whether its last move ended on the ground.
    // deaths:         times it died, for the dev panel.
    struct Enemy
    {
        lockstep::world::EntityId entity{};
        lockstep::physics::ControllerId controller{};
        EnemyPhase phase{EnemyPhase::Dead};
        int32_t health{0};
        float respawnTimer{0.0f};
        float attackCooldown{0.0f};
        float verticalSpeed{0.0f};
        bool isGrounded{false};
        uint32_t deaths{0};
    };

    // Enemy tuning.
    // walkSpeed:      metres per second of World time.
    // stopDistance:   it stops walking this close (horizontally) to its target.
    // attackRange:    it strikes a target this close (horizontally, centre to centre).
    // attackHeight:   largest height difference between centres it can strike across.
    // attackDamage:   hit points one strike takes.
    // attackCooldown: World seconds between strikes.
    // maxHealth:      hits it takes to die.
    // respawnDelay:   World seconds from death to respawn.
    // knockback:      metres a hit pushes it along the shot.
    // gravity:        downward acceleration while airborne, in m/s^2.
    // spawnPoint:     where its feet appear.
    struct EnemySettings
    {
        float walkSpeed{2.0f};
        float stopDistance{1.2f};
        float attackRange{1.6f};
        float attackHeight{1.5f};
        int32_t attackDamage{25};
        float attackCooldown{1.0f};
        int32_t maxHealth{3};
        float respawnDelay{3.0f};
        float knockback{0.3f};
        float gravity{18.0f};
        math::Vec3 spawnPoint{0.0f, 0.0f, -15.0f};
    };

    // A player the enemy may chase and strike.
    // position:  the pawn's centre.
    // pawnIndex: which pawn it is, handed back by enemyAttack().
    struct EnemyTarget
    {
        math::Vec3 position{0.0f, 0.0f, 0.0f};
        std::size_t pawnIndex{0};
    };

    // Host: creates the enemy entity and controller at the spawn point with full health.
    // enemy:       the enemy to bring alive. Must be dead.
    // entities:    the entity store.
    // replication: tags the entity as Archetype::Enemy.
    // physics:     creates the controller.
    // settings:    spawn point and health.
    // Returns:     success, or an error if the entity or controller could not be created.
    [[nodiscard]] lockstep::Status spawnEnemy(Enemy& enemy, lockstep::world::Entities& entities,
        lockstep::net::Replication& replication, lockstep::physics::PhysicsWorld& physics,
        const EnemySettings& settings);

    // Host: advances the enemy one tick: walks it toward the nearest target and lets it fall
    // while alive, counts down and respawns it while dead.
    // enemy:       the enemy.
    // entities:    the entity store.
    // replication: for tagging the respawned entity.
    // physics:     moves the controller.
    // settings:    tuning.
    // targets:     the players it may chase.
    // delta:       the World domain's delta for this tick, in seconds.
    // Returns:     success, or an error if a respawn failed (it is retried next tick).
    [[nodiscard]] lockstep::Status updateEnemy(Enemy& enemy, lockstep::world::Entities& entities,
        lockstep::net::Replication& replication, lockstep::physics::PhysicsWorld& physics,
        const EnemySettings& settings, std::span<const EnemyTarget> targets, float delta);

    // Host: counts down the strike cooldown and strikes the nearest target in melee range when
    // it is ready.
    // enemy:    the enemy.
    // entities: the entity store.
    // settings: tuning.
    // targets:  the players it may strike.
    // delta:    the World domain's delta for this tick, in seconds.
    // Returns:  the pawn index of the target struck this tick, or nothing.
    [[nodiscard]] std::optional<std::size_t> enemyAttack(Enemy& enemy, const lockstep::world::Entities& entities,
        const EnemySettings& settings, std::span<const EnemyTarget> targets, float delta);

    // Host: applies one hit. At zero health the enemy dies: its entity and controller are
    // destroyed and the respawn timer starts.
    // enemy:     the enemy, which must be alive.
    // entities:  the entity store.
    // physics:   for the knockback and removing the controller.
    // settings:  tuning.
    // direction: unit direction of the shot.
    // delta:     the World domain's delta for this tick, in seconds.
    // Returns:   true if this hit killed it.
    bool damageEnemy(Enemy& enemy, lockstep::world::Entities& entities, lockstep::physics::PhysicsWorld& physics,
        const EnemySettings& settings, const math::Vec3& direction, float delta);

    // Returns: a readable name for `phase`, for logs and the dev panel.
    [[nodiscard]] const char* toString(EnemyPhase phase) noexcept;
} //namespace game

#endif //LOCKSTEP_GAME_ENEMIES_ENEMY_HPP
