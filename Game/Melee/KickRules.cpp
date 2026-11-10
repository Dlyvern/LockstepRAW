#include "Melee/KickRules.hpp"

#include "GameState.hpp"
#include "Npcs/NpcRoster.hpp"
#include "Player/PawnKick.hpp"
#include "Player/PawnRays.hpp"

#include <Lockstep/Physics/PhysicsWorld.hpp>
#include <Lockstep/World/Entities.hpp>
#include <Lockstep/World/TimeDomains.hpp>

#include <algorithm>
#include <array>
#include <cmath>

namespace game
{
    namespace
    {
        namespace physics = lockstep::physics;
        namespace world = lockstep::world;

        using lockstep::app::EngineContext;
        using lockstep::app::GameState;

        constexpr math::Vec3 k_up{0.0f, 1.0f, 0.0f};
        constexpr float k_characterRadius{0.35f};
        constexpr float k_closeDistance{0.6f};
        constexpr std::array<float, 3> k_surfaceRayHeights{0.45f, 0.85f, 1.25f};
        constexpr float k_surfaceReachMargin{0.1f};

        struct KickShape
        {
            math::Vec3 foot{0.0f, 0.0f, 0.0f};
            math::Vec3 origin{0.0f, 0.0f, 0.0f};
            math::Vec3 forward{0.0f, 0.0f, -1.0f};
            float range{1.0f};
            float coneCos{0.8f};
        };

        bool isInCone(const KickShape& kick, const math::Vec3& point, float radius)
        {
            math::Vec3 offset = point - kick.foot;
            offset.y = 0.0f;
            const float distance = math::length(offset);

            if (distance > kick.range + radius)
            {
                return false;
            }

            return distance < k_closeDistance || math::dot(offset * (1.0f / distance), kick.forward) >= kick.coneCos;
        }

        bool kickNpcs(GameState& state, EngineContext& engine, std::size_t pawnIndex, const KickShape& kick)
        {
            const KickSettings& settings = state.settings.pawns.kick;
            NpcRoster& roster = state.simulation.npcs;
            Pawn& pawn = state.simulation.pawns[pawnIndex];
            const NpcTargetRef attacker{.kind = NpcTargetRef::Kind::Pawn, .index = static_cast<uint32_t>(pawnIndex)};
            const float delta = engine.timeDomains.delta(world::TimeDomain::Player);
            bool isHit = false;

            for (std::size_t index = 0; index < roster.npcs.size(); ++index)
            {
                Npc& npc = roster.npcs[index];
                const world::Entity* entity = engine.entities.find(npc.entity);

                if (!isNpcAlive(npc) || entity == nullptr || !isInCone(kick, entity->transform().position,
                        k_characterRadius))
                {
                    continue;
                }

                isHit = true;
                const physics::ControllerId controller = npc.controller;

                if (npc.faction == NpcFaction::Merc)
                {
                    ++pawn.hitCount;
                    ++pawn.hits;
                    const bool killed = damageNpc(roster, index, settings.damage, kick.origin, attacker,
                        engine.entities, engine.physics, state.settings.npcs);
                    pawn.killCount = static_cast<uint8_t>(pawn.killCount + (killed ? 1 : 0));

                    if (killed)
                    {
                        continue;
                    }

                    flinchNpc(npc, lockstep::anim::Limb::Torso,
                        std::max(settings.damage, state.settings.npcs.staggerDamage), state.settings.npcs);
                }

                static_cast<void>(engine.physics.move(controller, kick.forward * settings.shove, delta));
            }

            return isHit;
        }

        bool kickToys(GameState& state, EngineContext& engine, const KickShape& kick)
        {
            if (!state.room)
            {
                return false;
            }

            const KickSettings& settings = state.settings.pawns.kick;
            bool isHit = false;

            for (Toy& toy : state.room->toys)
            {
                const std::optional<math::Transform> body = engine.physics.transform(toy.body);

                if (!body || !isInCone(kick, body->position, toy.radius))
                {
                    continue;
                }

                const math::Vec3 velocity = engine.physics.linearVelocity(toy.body).value_or(math::Vec3{});
                isHit = engine.physics.setVelocity(toy.body,
                    velocity + kick.forward * settings.bodySpeed + k_up * settings.bodyLift) || isHit;
            }

            return isHit;
        }

        bool kickSurfaces(GameState& state, EngineContext& engine, const KickShape& kick,
            physics::ControllerId controller)
        {
            bool isHit = false;

            for (const float height : k_surfaceRayHeights)
            {
                const auto hit = sharedRaycast(engine.physics, kick.foot + k_up * height, kick.forward,
                    kick.range + k_surfaceReachMargin, controller, {});

                if (!hit)
                {
                    continue;
                }

                isHit = true;

                if (state.level)
                {
                    static_cast<void>(shatterGlass(state.glass, *state.level, engine.physics, hit->body, hit->point,
                        kick.forward));
                }
            }

            return isHit;
        }
    } //namespace

    void resolvePawnKick(GameState& state, EngineContext& engine, std::size_t pawnIndex)
    {
        const Pawn& pawn = state.simulation.pawns[pawnIndex];
        const PawnSettings& settings = state.settings.pawns;
        const world::Entity* entity = engine.entities.find(pawn.entity);

        if (entity == nullptr || !isKickImpact(pawn.motion, settings, engine.timeDomains.delta(world::TimeDomain::Player)))
        {
            return;
        }

        const math::Vec3 foot = pawnFootPosition(entity->transform());
        const math::Vec3 forward{-std::sin(pawn.command.yaw), 0.0f, -std::cos(pawn.command.yaw)};
        const KickShape kick{.foot = foot,
            .origin = foot + k_up * settings.kick.height,
            .forward = forward,
            .range = settings.kick.range,
            .coneCos = settings.kick.coneCos};
        const bool isNpcHit = kickNpcs(state, engine, pawnIndex, kick);
        const bool isToyHit = kickToys(state, engine, kick);
        const bool isSurfaceHit = kickSurfaces(state, engine, kick, pawn.controller);
        state.simulation.kickEvents.push_back(KickEvent{.pawn = static_cast<uint8_t>(pawnIndex),
            .origin = kick.origin,
            .direction = forward,
            .isHit = isNpcHit || isToyHit || isSurfaceHit});
    }
} //namespace game
