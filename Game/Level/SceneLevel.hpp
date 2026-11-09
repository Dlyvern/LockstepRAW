#ifndef LOCKSTEP_GAME_LEVEL_SCENE_LEVEL_HPP
#define LOCKSTEP_GAME_LEVEL_SCENE_LEVEL_HPP

#include "Player/Pawns.hpp"

#include <Lockstep/Assets/Scene.hpp>
#include <Lockstep/Assets/SceneCollision.hpp>
#include <Lockstep/Core/Math.hpp>
#include <Lockstep/Core/Result.hpp>

#include <array>
#include <filesystem>
#include <optional>
#include <span>

namespace lockstep::physics
{
    class PhysicsWorld;
} //namespace lockstep::physics

namespace lockstep::render
{
    class Renderer;
} //namespace lockstep::render

namespace game
{
    namespace math = lockstep::math;

    // A level loaded from a glTF file instead of the arena: `--scene <path>`, optionally with
    // `--scene-scale <factor>`. Host and client load the same file locally.
    // path:  the .gltf or .glb file.
    // scale: uniform scale applied to the scene.
    struct SceneLaunch
    {
        std::filesystem::path path;
        float scale{1.0f};
    };

    // An imported level: what is drawn and the static colliders that make it walkable.
    struct SceneLevel
    {
        lockstep::assets::Scene scene;
        lockstep::assets::SceneCollision collision;
    };

    // Where players and the enemy appear in a level.
    // pawns: foot positions of Player1's and Player2's pawns.
    // enemy: foot position of the enemy.
    struct LevelSpawns
    {
        std::array<math::Vec3, k_playerCount> pawns{};
        math::Vec3 enemy{0.0f, 0.0f, 0.0f};
    };

    // Reads `--scene` and `--scene-scale` from the command line.
    // arguments: command-line options.
    // Returns:   the scene to load, nothing without `--scene`, or an error for a missing value or
    //            a scale that is not a positive number.
    [[nodiscard]] lockstep::Result<std::optional<SceneLaunch>> parseSceneLaunch(std::span<const char* const> arguments);

    // Imports the scene, builds its colliders and logs load time, triangles, texture memory and
    // import warnings.
    // launch:   the file and scale.
    // renderer: uploads the scene.
    // physics:  receives the colliders.
    // Returns:  the level, or the import or collision error.
    [[nodiscard]] lockstep::Result<SceneLevel> loadSceneLevel(const SceneLaunch& launch,
        lockstep::render::Renderer& renderer, lockstep::physics::PhysicsWorld& physics);

    // Finds spawn points: at a node named "PlayerStart" or "Spawn" if the scene has one, else on
    // the ground near the centre of the scene's bounds: the lowest floor with room for a player
    // above it (not a roof or the underside of a floor slab), probed on a small grid around the
    // centre because the ground may have holes (under buildings).
    // Each point is dropped onto the floor below it with a raycast.
    // level:   the loaded level (its colliders must exist).
    // physics: the physics world for the raycasts.
    // Returns: the spawn points.
    [[nodiscard]] LevelSpawns findLevelSpawns(const SceneLevel& level, const lockstep::physics::PhysicsWorld& physics);
} //namespace game

#endif //LOCKSTEP_GAME_LEVEL_SCENE_LEVEL_HPP
