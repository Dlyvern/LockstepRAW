#include "Audio/Surfaces.hpp"

#include "Level/SceneLevel.hpp"

#include <Lockstep/Assets/SceneCollision.hpp>
#include <Lockstep/Physics/PhysicsWorld.hpp>

#include <algorithm>
#include <array>
#include <cctype>
#include <string>

namespace game
{
    namespace
    {
        namespace assets = lockstep::assets;
        namespace physics = lockstep::physics;

        constexpr float k_rayStartAbove{0.3f};
        constexpr float k_rayLength{0.8f};
        constexpr math::Vec3 k_down{0.0f, -1.0f, 0.0f};

        struct SurfaceKeyword
        {
            std::string_view keyword;
            Surface surface{Surface::Concrete};
        };

        constexpr std::array<SurfaceKeyword, 27> k_keywords{{
            {"asphalt", Surface::Asphalt},
            {"road", Surface::Asphalt},
            {"parking", Surface::Asphalt},
            {"street", Surface::Asphalt},
            {"grass", Surface::Grass},
            {"lawn", Surface::Grass},
            {"turf", Surface::Grass},
            {"dandelion", Surface::Grass},
            {"tile", Surface::Tile},
            {"ceramic", Surface::Tile},
            {"bathroom", Surface::Tile},
            {"wood", Surface::Wood},
            {"laminate", Surface::Wood},
            {"timber", Surface::Wood},
            {"parquet", Surface::Wood},
            {"plank", Surface::Wood},
            {"metal", Surface::Metal},
            {"steel", Surface::Metal},
            {"iron", Surface::Metal},
            {"grate", Surface::Metal},
            {"rail", Surface::Metal},
            {"concrete", Surface::Concrete},
            {"pavement", Surface::Concrete},
            {"sidewalk", Surface::Concrete},
            {"stair", Surface::Concrete},
            {"brick", Surface::Concrete},
            {"stone", Surface::Concrete},
        }};

        std::string lowerCase(std::string_view text)
        {
            std::string lower(text);
            std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char character)
            {
                return static_cast<char>(std::tolower(character));
            });

            return lower;
        }
    } //namespace

    const char* surfaceName(Surface surface)
    {
        switch (surface)
        {
            case Surface::Tile:
                return "tile";
            case Surface::Wood:
                return "wood";
            case Surface::Metal:
                return "metal";
            case Surface::Grass:
                return "grass";
            case Surface::Asphalt:
                return "asphalt";
            case Surface::Concrete:
            case Surface::Count:
                break;
        }

        return "concrete";
    }

    std::optional<Surface> surfaceFromName(std::string_view name)
    {
        const std::string lower = lowerCase(name);

        for (const SurfaceKeyword& entry : k_keywords)
        {
            if (lower.find(entry.keyword) != std::string::npos)
            {
                return entry.surface;
            }
        }

        return std::nullopt;
    }

    Surface surfaceBelow(const SceneLevel* level, const physics::PhysicsWorld& physics, const math::Vec3& foot,
        physics::ControllerId controller)
    {
        if (level == nullptr)
        {
            return Surface::Concrete;
        }

        const std::optional<physics::RaycastHit> hit = physics.raycast(foot - k_down * k_rayStartAbove, k_down,
            k_rayLength, physics::RaycastIgnore{.controller = controller});

        if (!hit || hit->body.isNull())
        {
            return Surface::Concrete;
        }

        const std::optional<uint32_t> instance = assets::findBodyInstance(level->collision, hit->body);

        if (!instance || *instance >= level->scene.instances.size())
        {
            return Surface::Concrete;
        }

        uint32_t node = level->scene.instances[*instance].node;

        while (node != assets::k_noNode && node < level->scene.nodes.size())
        {
            const assets::SceneNode& sceneNode = level->scene.nodes[node];

            if (const std::optional<Surface> surface = surfaceFromName(sceneNode.name))
            {
                return *surface;
            }

            node = sceneNode.parent;
        }

        return Surface::Concrete;
    }
} //namespace game
