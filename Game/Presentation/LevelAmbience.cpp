#include "Presentation/LevelAmbience.hpp"

#include <Lockstep/Core/Bounds.hpp>
#include <Lockstep/Render/Lighting.hpp>
#include <Lockstep/Render/Renderer.hpp>

#include <string_view>
#include <vector>

namespace game
{
    namespace
    {
        namespace assets = lockstep::assets;
        namespace render = lockstep::render;
        namespace math = lockstep::math;

        constexpr std::string_view k_interiorZonePrefix{"Zone_Interior_"};
        constexpr math::Aabb k_unitCube{.min = {-1.0f, -1.0f, -1.0f}, .max = {1.0f, 1.0f, 1.0f}};
    } //namespace

    uint32_t applyLevelAmbientZones(render::Renderer& renderer, const assets::Scene& scene)
    {
        std::vector<render::AmbientZone> zones;

        for (const assets::SceneNode& node : scene.nodes)
        {
            if (std::string_view(node.name).starts_with(k_interiorZonePrefix))
            {
                zones.push_back({.bounds = math::transformBounds(k_unitCube, node.transform)});
            }
        }

        renderer.setAmbientZones(zones);

        return static_cast<uint32_t>(zones.size());
    }
} //namespace game
