#include "Weapons/Penetration.hpp"

#include "Level/SceneLevel.hpp"

#include <Lockstep/Assets/SceneCollision.hpp>
#include <Lockstep/Physics/PhysicsWorld.hpp>

#if LOCKSTEP_DEV_TOOLS
#include <imgui.h>
#endif

#include <algorithm>
#include <cctype>
#include <optional>
#include <string>
#include <string_view>

namespace game
{
    namespace
    {
        namespace assets = lockstep::assets;
        namespace physics = lockstep::physics;

        constexpr float k_insideStep{0.002f};
        constexpr float k_exitStep{0.003f};
        constexpr float k_sameFace{0.001f};
        constexpr uint32_t k_maxBackSteps{4};
        constexpr std::size_t k_maxBreaks{4};

        struct MaterialKeyword
        {
            std::string_view keyword;
            ShotMaterial material{ShotMaterial::Hard};
        };

        constexpr MaterialKeyword k_keywords[] = {
            {"glass", ShotMaterial::Glass},
            {"window", ShotMaterial::Glass},
            {"mirror", ShotMaterial::Glass},
            {"windshield", ShotMaterial::Glass},
            {"concrete", ShotMaterial::Hard},
            {"brick", ShotMaterial::Hard},
            {"bld_", ShotMaterial::Hard},
            {"ext_", ShotMaterial::Hard},
            {"floor", ShotMaterial::Hard},
            {"ground", ShotMaterial::Hard},
            {"stair", ShotMaterial::Hard},
            {"foundation", ShotMaterial::Hard},
            {"column", ShotMaterial::Hard},
            {"pillar", ShotMaterial::Hard},
            {"alley", ShotMaterial::Hard},
            {"backdrop", ShotMaterial::Hard},
            {"metal", ShotMaterial::Metal},
            {"steel", ShotMaterial::Metal},
            {"iron", ShotMaterial::Metal},
            {"locker", ShotMaterial::Metal},
            {"shelves", ShotMaterial::Metal},
            {"cart", ShotMaterial::Metal},
            {"vending", ShotMaterial::Metal},
            {"dumpster", ShotMaterial::Metal},
            {"veh_", ShotMaterial::Metal},
            {"car_", ShotMaterial::Metal},
            {"truck", ShotMaterial::Metal},
            {"lift", ShotMaterial::Metal},
            {"shutter", ShotMaterial::Metal},
            {"fence", ShotMaterial::Metal},
            {"gate", ShotMaterial::Metal},
            {"pipe", ShotMaterial::Metal},
            {"extinguisher", ShotMaterial::Metal},
            {"generator", ShotMaterial::Metal},
            {"hydrant", ShotMaterial::Metal},
            {"wheelchair", ShotMaterial::Metal},
            {"bed_frame", ShotMaterial::Metal},
            {"wood", ShotMaterial::Wood},
            {"door", ShotMaterial::Wood},
            {"crate", ShotMaterial::Wood},
            {"pallet", ShotMaterial::Wood},
            {"plank", ShotMaterial::Wood},
            {"board", ShotMaterial::Wood},
            {"table", ShotMaterial::Wood},
            {"cabinet", ShotMaterial::Wood},
            {"drawer", ShotMaterial::Wood},
            {"cardboard", ShotMaterial::Soft},
            {"box", ShotMaterial::Soft},
            {"trash", ShotMaterial::Soft},
            {"paper", ShotMaterial::Soft},
            {"curtain", ShotMaterial::Soft},
            {"plastic", ShotMaterial::Soft},
            {"sofa", ShotMaterial::Soft},
            {"mattress", ShotMaterial::Soft},
            {"chair", ShotMaterial::Soft},
            {"plant", ShotMaterial::Soft},
            {"binder", ShotMaterial::Soft},
            {"wall", ShotMaterial::Drywall},
            {"partition", ShotMaterial::Drywall},
            {"plaster", ShotMaterial::Drywall},
            {"ceiling", ShotMaterial::Drywall},
        };

        std::optional<ShotMaterial> materialFromName(std::string_view name)
        {
            std::string lower(name);
            std::transform(lower.begin(), lower.end(), lower.begin(),
                [](unsigned char character) { return static_cast<char>(std::tolower(character)); });

            for (const MaterialKeyword& entry : k_keywords)
            {
                if (lower.find(entry.keyword) != std::string::npos)
                {
                    return entry.material;
                }
            }

            return std::nullopt;
        }

        std::optional<physics::RaycastHit> backTraceExit(const physics::PhysicsWorld& world,
            const physics::RaycastHit& entry, const math::Vec3& direction, float maxThickness)
        {
            math::Vec3 start = entry.point + direction * maxThickness;
            float remaining = maxThickness;
            physics::RaycastIgnore ignore{};

            for (uint32_t step = 0; step < k_maxBackSteps && remaining > 0.0f; ++step)
            {
                const std::optional<physics::RaycastHit> back = world.raycast(start, direction * -1.0f, remaining, ignore);

                if (!back)
                {
                    return std::nullopt;
                }

                if (back->body == entry.body && back->controller.isNull())
                {
                    if (math::length(back->point - entry.point) < k_sameFace)
                    {
                        return std::nullopt;
                    }

                    return back;
                }

                start = back->point;
                remaining -= back->distance;
                ignore = back->controller.isNull() ? physics::RaycastIgnore{.body = back->body}
                                                   : physics::RaycastIgnore{.controller = back->controller};
            }

            return std::nullopt;
        }

        std::optional<physics::RaycastHit> findExit(const physics::PhysicsWorld& world, const physics::RaycastHit& entry,
            const math::Vec3& direction, float maxThickness)
        {
            const std::optional<physics::RaycastHit> inside = world.raycast(entry.point + direction * k_insideStep,
                direction, maxThickness);

            if (inside && inside->controller.isNull() && inside->body == entry.body && inside->distance > k_sameFace)
            {
                return inside;
            }

            return backTraceExit(world, entry, direction, maxThickness);
        }

        ShotResult characterResult(const physics::RaycastHit& hit, float distance, physics::ControllerId enemy)
        {
            const bool isEnemy = !enemy.isNull() && hit.controller == enemy;

            return ShotResult{.outcome = isEnemy ? ShotOutcome::Enemy : ShotOutcome::Player,
                .distance = distance,
                .controller = hit.controller};
        }
    } //namespace

    ShotMaterial shotMaterialOfBody(const SceneLevel* level, physics::BodyId body)
    {
        if (level == nullptr || body.isNull())
        {
            return ShotMaterial::Soft;
        }

        const std::optional<uint32_t> instance = assets::findBodyInstance(level->collision, body);

        if (!instance || *instance >= level->scene.instances.size())
        {
            return ShotMaterial::Soft;
        }

        uint32_t node = level->scene.instances[*instance].node;

        while (node != assets::k_noNode && node < level->scene.nodes.size())
        {
            const assets::SceneNode& sceneNode = level->scene.nodes[node];

            if (const std::optional<ShotMaterial> material = materialFromName(sceneNode.name))
            {
                return *material;
            }

            node = sceneNode.parent;
        }

        return ShotMaterial::Hard;
    }

    const char* shotMaterialName(ShotMaterial material)
    {
        switch (material)
        {
            case ShotMaterial::Glass:
                return "glass";
            case ShotMaterial::Soft:
                return "soft";
            case ShotMaterial::Wood:
                return "wood";
            case ShotMaterial::Drywall:
                return "drywall";
            case ShotMaterial::Metal:
                return "metal";
            case ShotMaterial::Hard:
                return "hard";
            case ShotMaterial::Count:
                break;
        }

        return "?";
    }

    ShotPath traceShotPath(const physics::PhysicsWorld& world, const SceneLevel* level, const math::Vec3& origin,
        const math::Vec3& direction, float range, physics::ControllerId shooter, physics::ControllerId enemy,
        const PenetrationSettings& settings, const ShotBreaker& breaker)
    {
        ShotPath path{};
        math::Vec3 from = origin;
        float left = range;
        float power = settings.power;
        float damage = 1.0f;
        float travelled = 0.0f;
        std::size_t breaks = 0;
        const physics::RaycastIgnore ignore{.controller = shooter};

        while (left > 0.0f)
        {
            const std::optional<physics::RaycastHit> hit = world.raycast(from, direction, left, ignore);

            if (!hit)
            {
                path.segments.push_back(ShotSegment{.origin = from, .end = from + direction * left, .damageScale = damage});
                path.result = ShotResult{};
                return path;
            }

            if (!hit->controller.isNull())
            {
                path.segments.push_back(ShotSegment{.origin = from, .end = hit->point, .damageScale = damage});
                path.impacts.push_back(ShotImpact{.point = hit->point, .normal = hit->normal,
                    .controller = hit->controller});
                path.result = characterResult(*hit, travelled + hit->distance, enemy);
                return path;
            }

            if (breaker && breaks < k_maxBreaks && breaker(hit->body, hit->point, direction))
            {
                ++breaks;
                continue;
            }

            const ShotMaterial material = shotMaterialOfBody(level, hit->body);
            path.segments.push_back(ShotSegment{.origin = from, .end = hit->point, .damageScale = damage});
            path.impacts.push_back(ShotImpact{.point = hit->point, .normal = hit->normal, .body = hit->body,
                .material = material});
            travelled += hit->distance;
            path.result = ShotResult{.outcome = ShotOutcome::Level, .distance = travelled, .body = hit->body};

            const bool isFree = material == ShotMaterial::Glass;

            if (!isFree && path.penetrations >= settings.maxPenetrations)
            {
                return path;
            }

            const std::optional<physics::RaycastHit> exit = findExit(world, *hit, direction, settings.maxThickness);

            if (!exit)
            {
                return path;
            }

            const float thickness = math::length(exit->point - hit->point);
            const float cost = thickness * settings.cost[static_cast<std::size_t>(material)];

            if (cost >= power)
            {
                return path;
            }

            power -= cost;
            path.penetrations += isFree ? 0 : 1;
            damage *= settings.damageKeep * (settings.power > 0.0f ? power / settings.power : 0.0f);
            path.impacts.push_back(ShotImpact{.point = exit->point, .normal = direction, .body = hit->body,
                .material = material, .isExit = true});
            travelled += thickness;
            left = (left - hit->distance - thickness) * settings.rangeKeep;
            from = exit->point + direction * k_exitStep;
        }

        return path;
    }

#if LOCKSTEP_DEV_TOOLS
    void penetrationDevSection(PenetrationSettings& settings)
    {
        if (!ImGui::TreeNode("Penetration"))
        {
            return;
        }

        ImGui::SliderFloat("Power", &settings.power, 0.0f, 4.0f);
        int maxPenetrations = static_cast<int>(settings.maxPenetrations);

        if (ImGui::SliderInt("Max penetrations", &maxPenetrations, 0, 4))
        {
            settings.maxPenetrations = static_cast<uint32_t>(maxPenetrations);
        }

        ImGui::SliderFloat("Max thickness (m)", &settings.maxThickness, 0.02f, 1.0f);
        ImGui::SliderFloat("Damage kept per surface", &settings.damageKeep, 0.0f, 1.0f);
        ImGui::SliderFloat("Range kept per surface", &settings.rangeKeep, 0.0f, 1.0f);

        for (std::size_t index = 0; index < k_shotMaterialCount; ++index)
        {
            const std::string label = std::string("Cost per m, ") + shotMaterialName(static_cast<ShotMaterial>(index));
            ImGui::SliderFloat(label.c_str(), &settings.cost[index], 0.0f, 100.0f, "%.1f", ImGuiSliderFlags_Logarithmic);
        }

        ImGui::TreePop();
    }
#endif
} //namespace game
