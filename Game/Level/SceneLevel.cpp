#include "Level/SceneLevel.hpp"

#include <Lockstep/Assets/FileSystem.hpp>
#include <Lockstep/Physics/PhysicsWorld.hpp>

#include <algorithm>
#include <charconv>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <string>
#include <string_view>
#include <system_error>

namespace game
{
    namespace
    {
        namespace assets = lockstep::assets;
        namespace physics = lockstep::physics;

        constexpr std::string_view k_levelFlag{"--level"};
        constexpr std::string_view k_sceneFlag{"--scene"};
        constexpr std::string_view k_scaleFlag{"--scene-scale"};
        constexpr std::array<std::string_view, 2> k_spawnNodeNames{"PlayerStart", "Spawn"};
        constexpr std::array<std::string_view, k_playerCount> k_playerStartNames{"PlayerStart_1", "PlayerStart_2"};
        constexpr math::Vec3 k_down{0.0f, -1.0f, 0.0f};
        constexpr float k_dropHeight{1.5f};
        constexpr float k_dropDistance{20.0f};
        constexpr float k_footClearance{0.05f};
        constexpr float k_pawnSpacing{1.0f};
        constexpr float k_enemyDistance{5.0f};
        constexpr double k_bytesPerMegabyte{1024.0 * 1024.0};
        constexpr uint32_t k_maxFloorProbes{64};
        constexpr float k_probeStep{0.01f};
        constexpr float k_spawnHeadroom{2.0f};
        constexpr int k_groundSearchRings{3};
        constexpr float k_groundSearchStep{0.08f};
        constexpr float k_groundTolerance{0.5f};

        lockstep::Error invalid(std::string message)
        {
            return lockstep::Error{lockstep::ErrorCode::InvalidArgument, std::move(message)};
        }

        lockstep::Result<float> parseScale(std::string_view text)
        {
            float value = 0.0f;
            const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);

            if (error != std::errc{} || end != text.data() + text.size() || !(value > 0.0f) || !std::isfinite(value))
            {
                return invalid("--scene-scale: '" + std::string(text) + "' is not a positive number");
            }

            return float{value};
        }

        std::optional<math::Vec3> dropToFloor(const physics::PhysicsWorld& physics, const math::Vec3& from,
            float distance)
        {
            if (const std::optional<physics::RaycastHit> hit = physics.raycast(from, k_down, distance))
            {
                return hit->point + math::Vec3{0.0f, k_footClearance, 0.0f};
            }

            return std::nullopt;
        }

        math::Vec3 dropNear(const physics::PhysicsWorld& physics, const math::Vec3& point)
        {
            return dropToFloor(physics, point + math::Vec3{0.0f, k_dropHeight, 0.0f}, k_dropDistance).value_or(point);
        }

        std::optional<math::Vec3> lowestFloorBelow(const assets::Scene& scene, const physics::PhysicsWorld& physics,
            float x, float z)
        {
            math::Vec3 from{x, scene.bounds.max.y + 1.0f, z};
            std::optional<math::Vec3> lowest;

            for (uint32_t hitCount = 0; hitCount < k_maxFloorProbes; ++hitCount)
            {
                const std::optional<physics::RaycastHit> hit = physics.raycast(from, k_down,
                    from.y - scene.bounds.min.y + 1.0f);

                if (!hit)
                {
                    break;
                }

                const math::Vec3 foot = hit->point + math::Vec3{0.0f, k_footClearance, 0.0f};

                if (!physics.raycast(foot, -k_down, k_spawnHeadroom))
                {
                    lowest = foot;
                }

                from = hit->point + k_down * k_probeStep;
            }

            return lowest;
        }

        math::Vec3 groundNearCentre(const assets::Scene& scene, const physics::PhysicsWorld& physics)
        {
            const math::Vec3 centre = math::center(scene.bounds);
            const math::Vec3 extent = math::size(scene.bounds);
            std::optional<math::Vec3> best;

            for (int ring = 0; ring <= k_groundSearchRings; ++ring)
            {
                for (int stepX = -ring; stepX <= ring; ++stepX)
                {
                    for (int stepZ = -ring; stepZ <= ring; ++stepZ)
                    {
                        if (std::max(std::abs(stepX), std::abs(stepZ)) != ring)
                        {
                            continue;
                        }

                        const float x = centre.x + extent.x * k_groundSearchStep * static_cast<float>(stepX);
                        const float z = centre.z + extent.z * k_groundSearchStep * static_cast<float>(stepZ);
                        const std::optional<math::Vec3> floor = lowestFloorBelow(scene, physics, x, z);

                        if (floor && (!best || floor->y < best->y - k_groundTolerance))
                        {
                            best = floor;
                        }
                    }
                }
            }

            return best.value_or(centre);
        }

        void logLevel(const SceneLaunch& launch, const SceneLevel& level)
        {
            const assets::SceneStats& stats = level.scene.stats;
            const std::string name = launch.level.empty() ? launch.path.string() : launch.level;
            std::clog << std::fixed << std::setprecision(2) << "Game: " << (launch.level.empty() ? "scene" : "cooked level")
                << " '" << name << "' loaded in "
                << stats.loadSeconds << " s: " << stats.triangleCount << " triangles, " << stats.instanceCount
                << " instances of " << stats.meshCount << " meshes, " << stats.materialCount << " materials, "
                << stats.textureCount << " textures (" << stats.compressedTextures << " block compressed, "
                << static_cast<double>(stats.textureBytes) / k_bytesPerMegabyte << " MiB), "
                << static_cast<double>(stats.contentBytes) / k_bytesPerMegabyte << " MiB read, "
                << level.scene.lights.size() << " lights\n";
            const math::Aabb& bounds = level.scene.bounds;
            std::clog << "Game: scene bounds (" << bounds.min.x << ", " << bounds.min.y << ", " << bounds.min.z
                << ") to (" << bounds.max.x << ", " << bounds.max.y << ", " << bounds.max.z << ")\n";
            std::clog << "Game: scene collision: " << level.collision.bodies.size() << " bodies, "
                << level.collision.triangleCount << " triangles, " << level.collision.skipped << " instances skipped, "
                << level.collision.seconds << " s\n";

            for (const lockstep::anim::SkinnedModel& model : level.scene.skinnedModels)
            {
                std::clog << "Game: skinned model '" << model.name << "': " << model.skeleton.joints.size() << " joints, "
                    << model.parts.size() << " parts, " << model.clips.size() << " clips";

                for (const lockstep::anim::AnimationClip& clip : model.clips)
                {
                    std::clog << " '" << clip.name << "' " << clip.duration << " s";
                }

                std::clog << '\n';
            }

            for (const std::string& warning : level.scene.warnings)
            {
                std::clog << "Game: scene import: " << warning << '\n';
            }

            std::clog << std::defaultfloat;
        }
    } //namespace

    lockstep::Result<std::optional<SceneLaunch>> parseSceneLaunch(std::span<const char* const> arguments)
    {
        std::optional<SceneLaunch> launch;
        std::optional<float> scale;

        for (std::size_t index = 0; index < arguments.size(); ++index)
        {
            const std::string_view argument = arguments[index] != nullptr ? arguments[index] : "";

            if (argument != k_sceneFlag && argument != k_scaleFlag && argument != k_levelFlag)
            {
                continue;
            }

            if (index + 1 >= arguments.size() || arguments[index + 1] == nullptr)
            {
                return invalid(std::string(argument) + " needs a value");
            }

            const std::string_view value = arguments[++index];

            if (argument == k_sceneFlag || argument == k_levelFlag)
            {
                if (!launch)
                {
                    launch = SceneLaunch{};
                }

                if (argument == k_levelFlag)
                {
                    launch->level = value;
                }
                else
                {
                    launch->path = std::filesystem::path(value);
                }

                continue;
            }

            auto parsed = parseScale(value);

            if (!parsed)
            {
                return parsed.error();
            }

            scale = parsed.value();
        }

        if (launch && scale)
        {
            launch->scale = *scale;
        }

        return launch;
    }

    lockstep::Result<SceneLevel> loadSceneLevel(const SceneLaunch& launch, lockstep::render::Renderer& renderer,
        physics::PhysicsWorld& physics, const assets::FileSystem& files)
    {
        auto scene = launch.level.empty()
            ? assets::importGltf(renderer, launch.path, assets::SceneImportOptions{.scale = launch.scale})
            : assets::loadCookedScene(renderer, files, launch.level);

        if (!scene)
        {
            return scene.error();
        }

        SceneLevel level{.scene = std::move(scene).value(), .collision = {}};
        auto collision = assets::createSceneCollision(physics, level.scene);

        if (!collision)
        {
            return collision.error();
        }

        level.collision = std::move(collision).value();
        logLevel(launch, level);

        return level;
    }

    LevelSpawns findLevelSpawns(const SceneLevel& level, const physics::PhysicsWorld& physics)
    {
        std::optional<math::Transform> marker;

        for (const std::string_view name : k_spawnNodeNames)
        {
            if (!marker)
            {
                marker = assets::findNode(level.scene, name);
            }
        }

        const math::Vec3 base = marker ? dropNear(physics, marker->position) : groundNearCentre(level.scene, physics);
        const math::Quat rotation = marker ? marker->rotation : math::Quat{};
        const math::Vec3 right = math::transformDirection(math::toMat4(rotation), math::Vec3{1.0f, 0.0f, 0.0f});
        const math::Vec3 forward = math::transformDirection(math::toMat4(rotation), math::Vec3{0.0f, 0.0f, -1.0f});

        LevelSpawns spawns{};
        spawns.pawns[0] = dropNear(physics, base - right * k_pawnSpacing);
        spawns.pawns[1] = dropNear(physics, base + right * k_pawnSpacing);

        for (std::size_t pawn = 0; pawn < k_playerCount; ++pawn)
        {
            if (const std::optional<math::Transform> start = assets::findNode(level.scene, k_playerStartNames[pawn]))
            {
                spawns.pawns[pawn] = dropNear(physics, start->position);
            }
        }
        spawns.enemy = dropNear(physics, base + forward * k_enemyDistance);

        std::clog << "Game: level spawn at (" << base.x << ", " << base.y << ", " << base.z << ")"
            << (marker ? " from a spawn node\n" : " on the ground near the scene centre\n");

        return spawns;
    }
} //namespace game
