#include "Simulation/HostSimulation.hpp"

#include "GameState.hpp"
#include "Gore/GoreRules.hpp"
#include "Simulation/HospitalSimulation.hpp"
#include "Simulation/NpcSimulation.hpp"
#include "Simulation/PlayerCombat.hpp"
#include "Simulation/RoomSimulation.hpp"
#include "Simulation/StationSimulation.hpp"

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

        void logError(const char* what, const lockstep::Error& error)
        {
            std::clog << "Game: " << what << ": " << error.message << '\n';
        }

        std::vector<lockstep::physics::BodyId> dynamicBodies(const GameState& state)
        {
            std::vector<lockstep::physics::BodyId> bodies;

            if (state.room)
            {
                for (const Toy& toy : state.room->toys)
                {
                    if (!toy.body.isNull())
                    {
                        bodies.push_back(toy.body);
                    }
                }
            }

            return bodies;
        }

        void applyWeaponStance(GameState& state, const EngineContext& engine, std::size_t pawnIndex)
        {
            Pawn& pawn = state.simulation.pawns[pawnIndex];
            const world::Entity* entity = engine.entities.find(pawn.entity);
            const WeaponZone place = entity != nullptr ? weaponZoneAt(state.weaponZones, entity->transform().position)
                                                       : WeaponZone::Open;
            const WeaponZone zone = effectiveWeaponZone(place, state.simulation.isCombatActive);
            const bool isHoldingToy = state.room && state.room->state.heldToy[pawnIndex] != k_nobody;

            if (pawn.isDowned)
            {
                restrictDownedCommand(pawn.command, state.settings.downed.crawlScale);
            }

            pawn.isLowered = pawn.isArmed && isWeaponLowered(pawn.command.lower, zone, isHoldingToy);
            pawn.command.aim = pawn.command.aim && pawn.isArmed && !pawn.isLowered;
            pawn.command.isWeaponUp = pawn.isArmed && !pawn.isLowered;
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
                pawn.isArmed = state.settings.startArmed;

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
            const math::Vec3 origin = eyePosition(shooterEntity->transform());
            simulation.lastShot = traceShot(engine.physics, origin, direction, state.settings.hitscan.range,
                shooter.controller, simulation.enemy.controller);
            const float reach = simulation.lastShot.outcome == ShotOutcome::Miss ? state.settings.hitscan.range
                                                                                 : simulation.lastShot.distance;
            const bool isNpcHit = shootNpcs(state, engine, shooterIndex, origin, origin + direction * reach,
                simulation.lastShot.outcome == ShotOutcome::Player ? simulation.lastShot.controller
                                                                   : lockstep::physics::ControllerId{});
            restoreRewind(engine.physics, rewind);
            simulation.lastRewindTicks = rewind.ticks;
            ++shooter.shots;

            if (isNpcHit)
            {
                return;
            }

            if (simulation.lastShot.outcome == ShotOutcome::Level)
            {
                shootStation(state, shooterIndex, simulation.lastShot.body);
            }

            if (simulation.lastShot.outcome != ShotOutcome::Enemy)
            {
                std::clog << "Game: " << playerName(shooterIndex) << " shot: " << toString(simulation.lastShot.outcome)
                          << " at " << simulation.lastShot.distance << " m\n";
                return;
            }

            ++shooter.hits;
            ++shooter.hitCount;
            const bool killed = damageEnemy(simulation.enemy, engine.entities, engine.physics, state.settings.enemy,
                direction, engine.timeDomains.delta(world::TimeDomain::World));
            std::clog << "Game: " << playerName(shooterIndex) << " hit the enemy (health "
                      << simulation.enemy.health << ", rewound " << rewind.ticks << " ticks)"
                      << (killed ? ", enemy died" : "") << '\n';
            shooter.killCount = static_cast<uint8_t>(shooter.killCount + (killed ? 1 : 0));
        }

        void fireShots(GameState& state, EngineContext& engine, std::size_t shooterIndex, std::optional<double> viewTick,
            bool isReady)
        {
            Pawn& pawn = state.simulation.pawns[shooterIndex];

            while (pawn.hasCommand && pawn.processedShots != pawn.command.shotCount)
            {
                ++pawn.processedShots;

                if (!throwHeldToy(state, engine, shooterIndex) && pawn.isArmed && !pawn.isLowered && isReady
                    && !pawn.isDowned && !pawn.entity.isNull() && takePawnRound(state, shooterIndex))
                {
                    fireShot(state, engine, shooterIndex, viewTick);
                }
            }
        }

        void applyClientInputs(GameState& state, EngineContext& engine)
        {
            Pawn& pawn = state.simulation.pawns[k_clientPawn];
            const float delta = engine.timeDomains.delta(world::TimeDomain::Player);
            const std::vector<lockstep::physics::BodyId> ignored = dynamicBodies(state);

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
                        pawn.processedInteracts = command->interactCount;
                        pawn.ammo.processedReloads = command->reloadCount;
                    }

                    applyWeaponStance(state, engine, k_clientPawn);
                }

                const bool isReady = isWeaponReady(pawn.motion);

                if (world::Entity* entity = engine.entities.find(pawn.entity); entity != nullptr && pawn.hasCommand)
                {
                    movePawn(pawn, *entity, engine.physics, state.settings.pawns, delta, ignored);
                }

                fireShots(state, engine, k_clientPawn, input.viewTick, isReady);
                interactWithRoom(state, engine, k_clientPawn);
            }
        }

        void moveHostPawn(GameState& state, EngineContext& engine)
        {
            Pawn& pawn = state.simulation.pawns[k_hostPawn];
            const bool isReady = isWeaponReady(pawn.motion);

            if (world::Entity* entity = engine.entities.find(pawn.entity))
            {
                movePawn(pawn, *entity, engine.physics, state.settings.pawns,
                    engine.timeDomains.delta(world::TimeDomain::Player), dynamicBodies(state));
            }

            fireShots(state, engine, k_hostPawn, std::nullopt, isReady);
            interactWithRoom(state, engine, k_hostPawn);
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
                if (const world::Entity* attacker = engine.entities.find(enemy.entity))
                {
                    hurtPawn(state, engine, *struck, state.settings.enemy.attackDamage, attacker->transform().position);
                }
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
            std::vector<lockstep::physics::ControllerId> controllers = npcControllers(state);
            controllers.push_back(simulation.pawns[k_hostPawn].controller);
            controllers.push_back(simulation.pawns[k_clientPawn].controller);
            controllers.push_back(simulation.enemy.controller);
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
                state.local.statuses[index] = playerStatus(simulation.pawns[index], state.settings.downed);
            }

            state.local.isCombatActive = simulation.isCombatActive;
            state.local.isRestarting = simulation.isRestarting;

            if (engine.session.state() != net::ConnectionState::Connected)
            {
                simulation.sentStatuses.reset();
                return;
            }

            const StatusMessage status{.players = state.local.statuses,
                .isCombatActive = simulation.isCombatActive,
                .isRestarting = simulation.isRestarting};

            if (simulation.sentStatuses == status)
            {
                return;
            }

            if (auto sent = engine.session.send(encodeStatusMessage(status), net::Delivery::Reliable); !sent)
            {
                logError("status not sent", sent.error());
                return;
            }

            simulation.sentStatuses = status;
        }
    } //namespace

    lockstep::Status startHostSimulation(GameState& state, EngineContext& engine)
    {
        if (auto created = createArenaBodies(state.arena, engine.physics); !created)
        {
            return created.error();
        }

        state.simulation.pawns[k_hostPawn].isArmed = state.settings.startArmed;

        if (auto spawned = spawnPlayer(state, engine, k_hostPawn); !spawned)
        {
            return spawned.error();
        }

        std::clog << "Game: Player1 pawn spawned\n";

        if (!state.settings.hasEnemy)
        {
            return {};
        }

        return spawnEnemy(state.simulation.enemy, engine.entities, engine.replication, engine.physics,
            state.settings.enemy);
    }

    void runHostTick(GameState& state, EngineContext& engine)
    {
        Pawn& hostPawn = state.simulation.pawns[k_hostPawn];
        hostPawn.command = state.local.command;
        hostPawn.hasCommand = true;
        applyWeaponStance(state, engine, k_hostPawn);
        beginNpcTick(state);

        syncClientPawn(state, engine);
        applyClientInputs(state, engine);
        updateBulletTime(state, engine);
        moveHostPawn(state, engine);

        for (std::size_t index = 0; index < state.simulation.pawns.size(); ++index)
        {
            updatePawnAmmo(state, engine, index);
        }

        if (state.settings.hasEnemy)
        {
            updateEnemyOnHost(state, engine);
        }

        applyCorpseHits(state, engine);
        runNpcHostTick(state, engine);
        updateDownedPawns(state, engine);
        respawnPawns(state, engine);
        recordHittables(state, engine);
        publishStatuses(state, engine);
        publishClientState(state, engine);
        runRoomHostTick(state, engine);
        runStationHostTick(state, engine);
        runHospitalHostTick(state, engine);
    }
} //namespace game
