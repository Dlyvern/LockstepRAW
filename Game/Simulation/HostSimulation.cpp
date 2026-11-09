#include "Simulation/HostSimulation.hpp"

#include "GameState.hpp"

#include <Lockstep/Net/Replication.hpp>
#include <Lockstep/Net/Session.hpp>
#include <Lockstep/Physics/PhysicsWorld.hpp>
#include <Lockstep/World/Entities.hpp>
#include <Lockstep/World/TimeDomains.hpp>

#include <algorithm>
#include <array>
#include <iostream>
#include <optional>
#include <vector>

namespace game
{
    namespace
    {
        namespace net = lockstep::net;
        namespace world = lockstep::world;

        using lockstep::app::EngineContext;
        using lockstep::app::GameState;

        constexpr std::array<world::EntityOwner, k_playerCount> k_pawnOwners{world::EntityOwner::Player1,
            world::EntityOwner::Player2};
        constexpr double k_viewToShotTicks{0.5};

        const char* playerName(std::size_t pawnIndex)
        {
            return pawnIndex == k_hostPawn ? "Player1" : "Player2";
        }

        void logError(const char* what, const lockstep::Error& error)
        {
            std::clog << "Game: " << what << ": " << error.message << '\n';
        }

        lockstep::Status spawnPlayer(GameState& state, EngineContext& engine, std::size_t pawnIndex)
        {
            return spawnPawn(state.simulation.pawns[pawnIndex], engine.entities, engine.replication, engine.physics,
                k_pawnOwners[pawnIndex], state.settings.spawnPoints[pawnIndex], state.settings.pawns);
        }

        void syncClientPawn(GameState& state, EngineContext& engine)
        {
            Pawn& pawn = state.simulation.pawns[k_clientPawn];
            const bool isConnected = engine.session.state() == net::ConnectionState::Connected;

            if (isConnected && !pawn.isJoined)
            {
                pawn = Pawn{};

                if (auto spawned = spawnPlayer(state, engine, k_clientPawn); !spawned)
                {
                    logError("Player2 spawn failed", spawned.error());
                    return;
                }

                state.simulation.sentStatuses.reset();
                std::clog << "Game: Player2 pawn spawned\n";
            }
            else if (!isConnected && pawn.isJoined)
            {
                despawnPawn(pawn, engine.entities, engine.physics);
                pawn = Pawn{};
                std::clog << "Game: Player2 left, pawn removed\n";
            }
        }

        void updateBulletTime(GameState& state, EngineContext& engine)
        {
            GameState::Simulation& simulation = state.simulation;
            simulation.isRemoteBulletTime = simulation.pawns[k_clientPawn].hasCommand
                && simulation.pawns[k_clientPawn].command.bulletTime;
            const bool isBulletTime = state.local.command.bulletTime || simulation.isRemoteBulletTime;
            engine.timeDomains.setScale(world::TimeDomain::World,
                isBulletTime ? state.settings.bulletTimeScale : state.settings.worldScale);
        }

        void fireShot(GameState& state, EngineContext& engine, std::size_t shooterIndex, std::optional<double> viewTick)
        {
            GameState::Simulation& simulation = state.simulation;
            Pawn& shooter = simulation.pawns[shooterIndex];
            const world::Entity* shooterEntity = engine.entities.find(shooter.entity);

            if (shooterEntity == nullptr)
            {
                return;
            }

            Rewind rewind{};

            if (viewTick)
            {
                rewind = rewindTo(simulation.lagHistory, engine.physics, *viewTick - k_viewToShotTicks, shooter.controller);
            }

            const math::Vec3 direction = lookDirection(shooter.command.yaw, shooter.command.pitch);
            simulation.lastShot = traceShot(engine.physics, eyePosition(shooterEntity->transform()), direction,
                state.settings.hitscan.range, shooter.controller, simulation.enemy.controller);
            restoreRewind(engine.physics, rewind);
            simulation.lastRewindTicks = rewind.ticks;
            ++shooter.shots;

            if (simulation.lastShot.outcome != ShotOutcome::Enemy)
            {
                std::clog << "Game: " << playerName(shooterIndex) << " shot: " << toString(simulation.lastShot.outcome)
                          << " at " << simulation.lastShot.distance << " m\n";
                return;
            }

            ++shooter.hits;
            const bool killed = damageEnemy(simulation.enemy, engine.entities, engine.physics, state.settings.enemy,
                direction, engine.timeDomains.delta(world::TimeDomain::World));
            std::clog << "Game: " << playerName(shooterIndex) << " hit the enemy (health "
                      << simulation.enemy.health << ", rewound " << rewind.ticks << " ticks)"
                      << (killed ? ", enemy died" : "") << '\n';
        }

        void fireShots(GameState& state, EngineContext& engine, std::size_t shooterIndex, std::optional<double> viewTick)
        {
            Pawn& pawn = state.simulation.pawns[shooterIndex];

            while (pawn.hasCommand && pawn.processedShots != pawn.command.shotCount)
            {
                ++pawn.processedShots;
                fireShot(state, engine, shooterIndex, viewTick);
            }
        }

        void applyClientInputs(GameState& state, EngineContext& engine)
        {
            Pawn& pawn = state.simulation.pawns[k_clientPawn];
            const float delta = engine.timeDomains.delta(world::TimeDomain::Player);

            for (const net::RemoteInput& input : engine.replication.remoteInputs())
            {
                if (!pawn.isJoined)
                {
                    return;
                }

                if (const std::optional<PlayerCommand> command = decodeCommand(input.bytes))
                {
                    pawn.command = *command;
                    pawn.command.pitch = math::clamp(command->pitch, -state.settings.controls.maxPitch,
                        state.settings.controls.maxPitch);

                    if (!pawn.hasCommand)
                    {
                        pawn.hasCommand = true;
                        pawn.processedShots = command->shotCount;
                    }
                }

                if (world::Entity* entity = engine.entities.find(pawn.entity); entity != nullptr && pawn.hasCommand)
                {
                    movePawn(pawn, *entity, engine.physics, state.settings.pawns, delta);
                }

                fireShots(state, engine, k_clientPawn, input.viewTick);
            }
        }

        void moveHostPawn(GameState& state, EngineContext& engine)
        {
            Pawn& pawn = state.simulation.pawns[k_hostPawn];

            if (world::Entity* entity = engine.entities.find(pawn.entity))
            {
                movePawn(pawn, *entity, engine.physics, state.settings.pawns,
                    engine.timeDomains.delta(world::TimeDomain::Player));
            }

            fireShots(state, engine, k_hostPawn, std::nullopt);
        }

        std::vector<EnemyTarget> livingPawns(const GameState& state, const EngineContext& engine)
        {
            std::vector<EnemyTarget> targets;

            for (std::size_t index = 0; index < state.simulation.pawns.size(); ++index)
            {
                if (const world::Entity* entity = engine.entities.find(state.simulation.pawns[index].entity))
                {
                    targets.push_back(EnemyTarget{.position = entity->transform().position, .pawnIndex = index});
                }
            }

            return targets;
        }

        void damagePawn(GameState& state, EngineContext& engine, std::size_t pawnIndex, int32_t damage)
        {
            Pawn& pawn = state.simulation.pawns[pawnIndex];
            pawn.health = std::max(pawn.health - damage, 0);
            std::clog << "Game: enemy struck " << playerName(pawnIndex) << " (health " << pawn.health << ")\n";

            if (pawn.health > 0)
            {
                return;
            }

            despawnPawn(pawn, engine.entities, engine.physics);
            pawn.respawnTimer = state.settings.pawns.respawnDelay;
            ++pawn.deaths;
            std::clog << "Game: " << playerName(pawnIndex) << " died\n";
        }

        void updateEnemyOnHost(GameState& state, EngineContext& engine)
        {
            const std::vector<EnemyTarget> targets = livingPawns(state, engine);
            const float delta = engine.timeDomains.delta(world::TimeDomain::World);
            Enemy& enemy = state.simulation.enemy;
            const EnemyPhase phaseBefore = enemy.phase;

            if (auto updated = updateEnemy(enemy, engine.entities, engine.replication, engine.physics,
                    state.settings.enemy, targets, delta); !updated)
            {
                logError("enemy respawn failed", updated.error());
            }

            if (phaseBefore == EnemyPhase::Dead && enemy.phase == EnemyPhase::Alive)
            {
                std::clog << "Game: enemy respawned\n";
            }

            if (const std::optional<std::size_t> struck = enemyAttack(enemy, engine.entities, state.settings.enemy,
                    targets, delta))
            {
                damagePawn(state, engine, *struck, state.settings.enemy.attackDamage);
            }
        }

        void respawnPawns(GameState& state, EngineContext& engine)
        {
            const float delta = engine.timeDomains.delta(world::TimeDomain::Player);

            for (std::size_t index = 0; index < state.simulation.pawns.size(); ++index)
            {
                Pawn& pawn = state.simulation.pawns[index];

                if (!pawn.isJoined || !pawn.entity.isNull())
                {
                    continue;
                }

                pawn.respawnTimer = std::max(pawn.respawnTimer - delta, 0.0f);

                if (pawn.respawnTimer > 0.0f)
                {
                    continue;
                }

                if (auto spawned = spawnPlayer(state, engine, index); !spawned)
                {
                    logError("respawn failed", spawned.error());
                    continue;
                }

                std::clog << "Game: " << playerName(index) << " respawned\n";
            }
        }

        void recordHittables(GameState& state, const EngineContext& engine)
        {
            const GameState::Simulation& simulation = state.simulation;
            const std::array<lockstep::physics::ControllerId, k_playerCount + 1> controllers{
                simulation.pawns[k_hostPawn].controller, simulation.pawns[k_clientPawn].controller,
                simulation.enemy.controller};
            recordLagFrame(state.simulation.lagHistory, engine.physics, state.lastTick, controllers);
        }

        void publishClientState(const GameState& state, EngineContext& engine)
        {
            if (engine.session.state() != net::ConnectionState::Connected)
            {
                return;
            }

            const Pawn& pawn = state.simulation.pawns[k_clientPawn];
            const std::optional<math::Vec3> foot = engine.physics.footPosition(pawn.controller);
            const OwnPawnState own{.isAlive = foot.has_value(),
                .life = pawn.life,
                .foot = foot.value_or(math::Vec3{0.0f, 0.0f, 0.0f}),
                .motion = pawn.motion};

            if (auto set = engine.replication.setClientState(encodeOwnPawnState(own)); !set)
            {
                logError("client state not sent", set.error());
            }
        }

        void publishStatuses(GameState& state, EngineContext& engine)
        {
            GameState::Simulation& simulation = state.simulation;

            for (std::size_t index = 0; index < simulation.pawns.size(); ++index)
            {
                state.local.statuses[index] = playerStatus(simulation.pawns[index]);
            }

            if (engine.session.state() != net::ConnectionState::Connected)
            {
                simulation.sentStatuses.reset();
                return;
            }

            if (simulation.sentStatuses == state.local.statuses)
            {
                return;
            }

            const auto message = encodePlayerStatuses(state.local.statuses);

            if (auto sent = engine.session.send(message, net::Delivery::Reliable); !sent)
            {
                logError("health not sent", sent.error());
                return;
            }

            simulation.sentStatuses = state.local.statuses;
        }
    } //namespace

    lockstep::Status startHostSimulation(GameState& state, EngineContext& engine)
    {
        if (auto created = createArenaBodies(state.arena, engine.physics); !created)
        {
            return created.error();
        }

        if (auto spawned = spawnPlayer(state, engine, k_hostPawn); !spawned)
        {
            return spawned.error();
        }

        std::clog << "Game: Player1 pawn spawned\n";

        return spawnEnemy(state.simulation.enemy, engine.entities, engine.replication, engine.physics,
            state.settings.enemy);
    }

    void runHostTick(GameState& state, EngineContext& engine)
    {
        Pawn& hostPawn = state.simulation.pawns[k_hostPawn];
        hostPawn.command = state.local.command;
        hostPawn.hasCommand = true;

        syncClientPawn(state, engine);
        applyClientInputs(state, engine);
        updateBulletTime(state, engine);
        moveHostPawn(state, engine);
        updateEnemyOnHost(state, engine);
        respawnPawns(state, engine);
        recordHittables(state, engine);
        publishStatuses(state, engine);
        publishClientState(state, engine);
    }
} //namespace game
