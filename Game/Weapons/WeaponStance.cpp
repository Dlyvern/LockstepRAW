#include "Weapons/WeaponStance.hpp"

#include <Lockstep/Assets/Scene.hpp>

#include <algorithm>
#include <cmath>
#include <iostream>
#include <string_view>

namespace game
{
    namespace
    {
        constexpr std::string_view k_safePrefix{"Zone_Safe"};
        constexpr std::string_view k_hostilePrefix{"Zone_Hostile"};
        constexpr float k_minHalfExtent{1.0e-4f};

        bool isInside(const WeaponZoneBox& zone, const math::Vec3& position)
        {
            const math::Vec3 local = math::conjugate(math::normalize(zone.box.rotation)) * (position - zone.box.position);
            const math::Vec3 extent = zone.box.scale;

            return std::abs(local.x) <= std::max(std::abs(extent.x), k_minHalfExtent)
                && std::abs(local.y) <= std::max(std::abs(extent.y), k_minHalfExtent)
                && std::abs(local.z) <= std::max(std::abs(extent.z), k_minHalfExtent);
        }
    } //namespace

    std::vector<WeaponZoneBox> findWeaponZones(const lockstep::assets::Scene& scene)
    {
        std::vector<WeaponZoneBox> zones;

        for (const lockstep::assets::SceneNode& node : scene.nodes)
        {
            const std::string_view name = node.name;

            if (name.starts_with(k_safePrefix))
            {
                zones.push_back(WeaponZoneBox{.box = node.transform, .kind = WeaponZone::Safe});
            }
            else if (name.starts_with(k_hostilePrefix))
            {
                zones.push_back(WeaponZoneBox{.box = node.transform, .kind = WeaponZone::Hostile});
            }
        }

        std::clog << "Game: " << zones.size() << " weapon zone(s)\n";

        return zones;
    }

    WeaponZone weaponZoneAt(std::span<const WeaponZoneBox> zones, const math::Vec3& position)
    {
        WeaponZone found = WeaponZone::Open;

        for (const WeaponZoneBox& zone : zones)
        {
            if (!isInside(zone, position))
            {
                continue;
            }

            if (zone.kind == WeaponZone::Safe)
            {
                return WeaponZone::Safe;
            }

            found = zone.kind;
        }

        return found;
    }

    bool isWeaponLowered(bool wantsLowered, WeaponZone zone)
    {
        switch (zone)
        {
            case WeaponZone::Safe:
                return true;
            case WeaponZone::Hostile:
                return false;
            case WeaponZone::Open:
                break;
        }

        return wantsLowered;
    }
} //namespace game
