#include "Player/LocalPlayer.hpp"

#include "GameState.hpp"
#include "Player/PawnKick.hpp"
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

    PawnLook remotePawnLook(const GameState& state, world::EntityOwner owner, const math::Transform& transform)
    {
        const PlayerStatus& status = state.local.statuses[ownerPawnIndex(owner)];

        return PawnLook{.isArmed = status.isArmed && !status.isDowned, .isAiming = status.isArmed && status.isAiming,
            .isLowered = status.isArmed && status.isLowered,
            .crouch = pawnCrouchAmount(transform, state.settings.pawns),
            .isSliding = status.isSliding,
            .isVaulting = status.isVaulting,
            .isKicking = status.isKicking};
    }

    PawnLook localPawnLook(const GameState& state, const EngineContext& engine,
        const std::optional<math::Transform>& transform)
    {
        const PlayerStatus& status = state.local.statuses[localPawnIndex(engine)];
        const bool isArmed = status.isArmed && !status.isDowned;

        return PawnLook{.isArmed = isArmed,
            .isAiming = isArmed && isAimingReady(state.local.command, localPawnMotion(state, engine)),
            .isLowered = isArmed && isWeaponLowered(state.local.command.lower, state.local.weaponZone,
                isHoldingToy(state, engine)),
            .crouch = transform ? pawnCrouchAmount(*transform, state.settings.pawns) : 0.0f,
            .isSliding = isSliding(localPawnMotion(state, engine)),
            .isVaulting = isVaulting(localPawnMotion(state, engine)),
            .isKicking = isKicking(localPawnMotion(state, engine))};
    }

    const PawnMotion& localPawnMotion(const GameState& state, const EngineContext& engine)
    {
        return isClient(engine) ? state.local.prediction.motion : state.simulation.pawns[k_hostPawn].motion;
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

        const WeaponZone place = pawn != nullptr ? weaponZoneAt(state.weaponZones, pawn->transform().position)
                                                 : WeaponZone::Open;

        return effectiveWeaponZone(place, state.local.isCombatActive);
    }

    lockstep::physics::ControllerId localController(const GameState& state, const EngineContext& engine)
    {
        return isClient(engine) ? state.local.prediction.controller : state.simulation.pawns[k_hostPawn].controller;
    }
} //namespace game
