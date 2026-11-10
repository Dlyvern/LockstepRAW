#include "Simulation/PlayerCombat.hpp"

#include "GameState.hpp"
#include "Level/RoomState.hpp"
#include "Simulation/HostSimulation.hpp"
#include "Simulation/NpcSimulation.hpp"
#include "Weapons/Loadout.hpp"

#include <Lockstep/Physics/PhysicsWorld.hpp>
#include <Lockstep/World/Entities.hpp>
#include <Lockstep/World/TimeDomains.hpp>

#include <algorithm>
#include <array>
#include <iostream>

namespace game
{
    namespace
    {
        namespace world = lockstep::world;

        using lockstep::app::EngineContext;
        using lockstep::app::GameState;

        constexpr std::array<world::EntityOwner, k_playerCount> k_pawnOwners{world::EntityOwner::Player1,
            world::EntityOwner::Player2};

        void startRestart(GameState& state, const char* reason)
        {
            GameState::Simulation& simulation = state.simulation;

            if (simulation.isRestarting)
            {
                return;
            }

            simulation.isRestarting = true;
            simulation.restartSeconds = state.settings.downed.restartDelay;
            std::clog << "Game: squad failed (" << reason << "); restarting at the "
                      << (simulation.checkpoint ? "last checkpoint" : "level start") << '\n';
        }

        void restartAtCheckpoint(GameState& state, EngineContext& engine)
        {
            GameState::Simulation& simulation = state.simulation;
            simulation.isRestarting = false;

            for (std::size_t index = 0; index < simulation.pawns.size(); ++index)
            {
                Pawn& pawn = simulation.pawns[index];

                if (!pawn.isJoined)
                {
                    continue;
                }

                despawnPawn(pawn, engine.entities, engine.physics);
                clearDowned(pawn);
                const math::Vec3 foot = simulation.checkpoint ? (*simulation.checkpoint)[index]
                                                              : state.settings.spawnPoints[index];

                if (auto spawned = spawnPawn(pawn, engine.entities, engine.replication, engine.physics,
                        k_pawnOwners[index], foot, state.settings.pawns); !spawned)
                {
                    std::clog << "Game: " << playerName(index) << " not respawned: " << spawned.error().message << '\n';
                    pawn.respawnTimer = state.settings.pawns.respawnDelay;
                    continue;
                }

                if (pawn.isArmed)
                {
                    issueLoadout(pawn, state.settings.weapons);
                }
            }

            simulation.lagHistory = LagHistory{};
            restartNpcs(state, engine);
            std::clog << "Game: checkpoint restart done\n";
        }

        std::optional<math::Vec3> pawnFoot(const GameState& state, const EngineContext& engine, std::size_t index)
        {
            return engine.physics.footPosition(state.simulation.pawns[index].controller);
        }

        void updateRevive(GameState& state, const EngineContext& engine, std::size_t downedIndex, float delta)
        {
            Pawn& downed = state.simulation.pawns[downedIndex];
            const std::size_t partnerIndex = downedIndex == k_hostPawn ? k_clientPawn : k_hostPawn;
            const Pawn& partner = state.simulation.pawns[partnerIndex];
            const std::optional<math::Vec3> downedFoot = pawnFoot(state, engine, downedIndex);
            const std::optional<math::Vec3> partnerFoot = pawnFoot(state, engine, partnerIndex);
            const DownedSettings& settings = state.settings.downed;
            const bool isReviving = downedFoot && partnerFoot && !partner.isDowned && partner.hasCommand
                && partner.command.isInteractHeld && math::length(*partnerFoot - *downedFoot) <= settings.reviveRange;

            if (!isReviving)
            {
                downed.reviveSeconds = 0.0f;
                return;
            }

            downed.reviveSeconds += delta;

            if (downed.reviveSeconds >= settings.reviveSeconds)
            {
                revivePawn(downed, settings);
                std::clog << "Game: " << playerName(partnerIndex) << " revived " << playerName(downedIndex) << '\n';
            }
        }
    } //namespace

    void updatePawnAmmo(GameState& state, EngineContext& engine, std::size_t pawnIndex)
    {
        Pawn& pawn = state.simulation.pawns[pawnIndex];
        const float delta = engine.timeDomains.delta(world::TimeDomain::Player);

        if (!pawn.isArmed)
        {
            pawn.ammo.isIssued = false;
            pawn.ammo.reloadSeconds = 0.0f;
            pawn.switchSeconds = 0.0f;
            return;
        }

        if (!pawn.ammo.isIssued)
        {
            issueLoadout(pawn, state.settings.weapons);
        }

        pawn.switchSeconds = std::max(pawn.switchSeconds - delta, 0.0f);

        if (pawn.hasCommand && pawn.command.weapon != pawn.weapon && !pawn.isDowned && !pawn.entity.isNull())
        {
            switchWeapon(pawn, state.settings.weapons, pawn.command.weapon);
            std::clog << "Game: " << playerName(pawnIndex) << " switches to the " << weaponName(pawn.weapon) << '\n';
        }

        Ammo& ammo = pawn.ammo;
        const AmmoSettings& settings = weaponStats(state.settings.weapons, pawn.weapon).ammo;

        while (pawn.hasCommand && ammo.processedReloads != pawn.command.reloadCount)
        {
            ++ammo.processedReloads;

            if (!pawn.isDowned && !pawn.entity.isNull() && pawn.switchSeconds <= 0.0f && startReload(ammo, settings))
            {
                std::clog << "Game: " << playerName(pawnIndex) << " reloads\n";
            }
        }

        if (stepReload(ammo, settings, delta))
        {
            std::clog << "Game: " << playerName(pawnIndex) << " reloaded (" << ammo.magazine << " / " << ammo.reserve
                      << ")\n";
        }
    }

    bool takePawnRound(GameState& state, std::size_t pawnIndex)
    {
        Pawn& pawn = state.simulation.pawns[pawnIndex];
        const bool hadRounds = pawn.ammo.magazine > 0;
        const bool fired = takeRound(pawn.ammo, weaponStats(state.settings.weapons, pawn.weapon).ammo);

        if (!fired && !hadRounds && isReloading(pawn.ammo))
        {
            std::clog << "Game: " << playerName(pawnIndex) << " is empty, reloading\n";
        }

        return fired;
    }

    void hurtPawn(GameState& state, EngineContext& engine, std::size_t pawnIndex, int32_t damage,
        const math::Vec3& source)
    {
        Pawn& pawn = state.simulation.pawns[pawnIndex];
        const world::Entity* entity = engine.entities.find(pawn.entity);

        if (entity == nullptr || state.simulation.isRestarting)
        {
            return;
        }

        switch (applyPawnDamage(pawn, damage, source, entity->transform().position, state.settings.downed))
        {
            case PawnDamage::None:
                return;
            case PawnDamage::Hurt:
                if (!pawn.isDowned)
                {
                    std::clog << "Game: " << playerName(pawnIndex) << " hit (health " << pawn.health << ")\n";
                }

                return;
            case PawnDamage::Downed:
                ++pawn.deaths;
                std::clog << "Game: " << playerName(pawnIndex) << " is down (bleeds out in "
                          << state.settings.downed.bleedOutSeconds << " s)\n";
                return;
            case PawnDamage::BledOut:
                startRestart(state, "a player bled out");
                return;
        }
    }

    void updateDownedPawns(GameState& state, EngineContext& engine)
    {
        GameState::Simulation& simulation = state.simulation;
        const float delta = engine.timeDomains.delta(world::TimeDomain::Player);

        if (simulation.isRestarting)
        {
            simulation.restartSeconds -= delta;

            if (simulation.restartSeconds <= 0.0f)
            {
                restartAtCheckpoint(state, engine);
            }

            return;
        }

        std::size_t present = 0;
        std::size_t down = 0;

        for (std::size_t index = 0; index < simulation.pawns.size(); ++index)
        {
            Pawn& pawn = simulation.pawns[index];

            if (pawn.entity.isNull())
            {
                continue;
            }

            ++present;

            if (!pawn.isDowned)
            {
                continue;
            }

            if (stepBleedOut(pawn, delta))
            {
                std::clog << "Game: " << playerName(index) << " bled out\n";
                startRestart(state, "a player bled out");
                return;
            }

            updateRevive(state, engine, index, delta);
            down += pawn.isDowned ? 1 : 0;
        }

        if (present > 0 && down == present)
        {
            startRestart(state, present > 1 ? "both players are down" : "the player is down");
        }
    }
} //namespace game
