#include "Npcs/NpcFiring.hpp"

#include "Npcs/NpcRoster.hpp"

#include <Lockstep/Physics/PhysicsWorld.hpp>

#include <algorithm>
#include <cmath>
#include <optional>

namespace game
{
    namespace
    {
        namespace physics = lockstep::physics;

        constexpr float k_range{80.0f};
        constexpr float k_unsettledSpread{2.0f};
        constexpr float k_runSpeed{6.4f};
        constexpr float k_downedSpread{0.7f};

        float roughNormal(uint32_t& random)
        {
            return (randomUnit(random) + randomUnit(random) + randomUnit(random) - 1.5f) * 2.0f;
        }

        math::Vec3 aimError(const math::Vec3& direction, float spread, uint32_t& random)
        {
            const math::Vec3 helper = std::abs(direction.y) < 0.95f ? math::Vec3{0.0f, 1.0f, 0.0f}
                                                                    : math::Vec3{1.0f, 0.0f, 0.0f};
            const math::Vec3 right = math::normalize(math::cross(direction, helper));
            const math::Vec3 up = math::cross(right, direction);

            return math::normalize(direction + right * (roughNormal(random) * spread)
                + up * (roughNormal(random) * spread));
        }

        float spreadFor(const Npc& npc, const NpcActor& target, const NpcSettings& settings)
        {
            const float settle = settings.aimSettleSeconds > 0.0f
                ? std::max(1.0f - npc.aimSeconds / settings.aimSettleSeconds, 0.0f) : 0.0f;
            const float moving = 1.0f + (settings.movingTargetSpread - 1.0f) * std::min(target.speed / k_runSpeed, 1.0f);

            return fighterSettings(settings, npc.faction).spread * (1.0f + k_unsettledSpread * settle)
                * (1.0f + std::min(npc.suppression, 2.0f)) * moving * (target.isDowned ? k_downedSpread : 1.0f);
        }
    } //namespace

    bool fireNpcWeapon(NpcRoster& roster, std::size_t index, const NpcWorld& world, const NpcActor& target,
        const math::Vec3& eye)
    {
        Npc& npc = roster.npcs[index];
        const NpcSettings& settings = world.settings;
        npc.fireCooldown = std::max(npc.fireCooldown - world.delta, 0.0f);

        if (npc.fireCooldown > 0.0f || npc.rounds <= 0 || world.delta <= 0.0f)
        {
            return false;
        }

        if (npc.burstLeft <= 0)
        {
            npc.burstLeft = static_cast<int32_t>(randomRange(roster.random, static_cast<float>(settings.burstMin),
                static_cast<float>(settings.burstMax) + 1.0f));
        }

        const math::Vec3 offset = target.chest - eye;
        const float distance = math::length(offset);

        if (distance <= 0.0f)
        {
            return false;
        }

        const math::Vec3 direction = aimError(offset * (1.0f / distance), spreadFor(npc, target, settings),
            roster.random);
        const std::optional<physics::RaycastHit> hit = world.physics.raycast(eye, direction, k_range,
            physics::RaycastIgnore{.controller = npc.controller});
        const math::Vec3 end = hit ? hit->point : eye + direction * k_range;

        if (hit && !hit->controller.isNull())
        {
            for (const NpcActor& actor : world.actors)
            {
                if (actor.controller == hit->controller && isHostile(npc, actor))
                {
                    world.hits.push_back(NpcHit{.target = actor.ref,
                        .damage = fighterSettings(settings, npc.faction).damage,
                        .source = eye,
                        .shooter = index});
                    break;
                }
            }
        }

        roster.shots.push_back(NpcShotEvent{.origin = eye,
            .end = end,
            .isPolice = npc.faction == NpcFaction::Squad,
            .shooter = NpcTargetRef{.kind = NpcTargetRef::Kind::Npc, .index = static_cast<uint32_t>(index)}});
        ++npc.shotCount;
        --npc.rounds;
        --npc.burstLeft;
        npc.fireCooldown = npc.burstLeft > 0 ? settings.shotInterval
                                             : randomRange(roster.random, settings.burstPauseMin, settings.burstPauseMax);

        return true;
    }
} //namespace game
