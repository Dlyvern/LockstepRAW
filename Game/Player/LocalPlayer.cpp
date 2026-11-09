#include "Player/LocalPlayer.hpp"

#include "GameState.hpp"
#include "Simulation/HostSimulation.hpp"

#include <Lockstep/Net/Session.hpp>

namespace game
{
    namespace
    {
        namespace net = lockstep::net;
        namespace world = lockstep::world;

        using lockstep::app::EngineContext;
        using lockstep::app::GameState;

        bool isClient(const EngineContext& engine)
        {
            return engine.session.role() == net::Role::Client;
        }
    } //namespace

    std::size_t localPawnIndex(const EngineContext& engine)
    {
        return isClient(engine) ? k_clientPawn : k_hostPawn;
    }

    world::EntityOwner localOwner(const EngineContext& engine)
    {
        return isClient(engine) ? world::EntityOwner::Player2 : world::EntityOwner::Player1;
    }

    std::size_t ownerPawnIndex(world::EntityOwner owner)
    {
        return owner == world::EntityOwner::Player2 ? k_clientPawn : k_hostPawn;
    }

    PawnLook remotePawnLook(const GameState& state, world::EntityOwner owner)
    {
        const PlayerStatus& status = state.local.statuses[ownerPawnIndex(owner)];

        return PawnLook{.isArmed = status.isArmed, .isAiming = status.isArmed && status.isAiming,
            .isLowered = status.isArmed && status.isLowered};
    }

    PawnLook localPawnLook(const GameState& state, const EngineContext& engine)
    {
        const bool isArmed = state.local.statuses[localPawnIndex(engine)].isArmed;

        return PawnLook{.isArmed = isArmed, .isAiming = isArmed && state.local.command.aim,
            .isLowered = isArmed && isWeaponLowered(state.local.command.lower, state.local.weaponZone)};
    }

    bool isHoldingToy(const GameState& state, const EngineContext& engine)
    {
        return state.room && state.room->state.heldToy[localPawnIndex(engine)] != k_nobody;
    }

    WeaponZone localWeaponZone(const GameState& state, const EngineContext& engine)
    {
        const world::EntityId entity = isClient(engine) ? state.local.prediction.entity
                                                        : state.simulation.pawns[k_hostPawn].entity;
        const world::Entity* pawn = engine.entities.find(entity);

        return pawn != nullptr ? weaponZoneAt(state.weaponZones, pawn->transform().position) : WeaponZone::Open;
    }

    lockstep::physics::ControllerId localController(const GameState& state, const EngineContext& engine)
    {
        return isClient(engine) ? state.local.prediction.controller : state.simulation.pawns[k_hostPawn].controller;
    }
} //namespace game
