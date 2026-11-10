#ifndef LOCKSTEP_GAME_LEVEL_SCENE_LEVEL_HPP
#define LOCKSTEP_GAME_LEVEL_SCENE_LEVEL_HPP

#include "Player/Pawns.hpp"

#include <Lockstep/Assets/Scene.hpp>
#include <Lockstep/Assets/SceneCollision.hpp>
#include <Lockstep/Core/Math.hpp>
#include <Lockstep/Core/Result.hpp>
#include <Lockstep/Physics/PhysicsTypes.hpp>

#include <array>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace lockstep::physics
{
    class PhysicsWorld;
} //namespace lockstep::physics

namespace lockstep::render
{
    class Renderer;
} //namespace lockstep::render

namespace lockstep::assets
{
    class FileSystem;
} //namespace lockstep::assets

namespace game
{
    namespace math = lockstep::math;

    // A level loaded instead of the arena: a cooked level from the mounted content,
    // `--level <virtual path>` (with `--content <folder|pak>`), or a glTF file directly,
    // `--scene <path>` optionally with `--scene-scale <factor>`. Host and client load the same
    // level locally.
    // level: virtual path of a cooked level file ("levels/sponza/scene"); wins over `path`.
    // path:  the .gltf or .glb file, when `level` is empty.
    // scale: uniform scale applied to a glTF scene (a cooked level has its scale baked in).
    struct SceneLaunch
    {
        std::string level;
        std::filesystem::path path;
        float scale{1.0f};
    };

    // An imported level: what is drawn and the static colliders that make it walkable.
    // blockers: invisible static boxes from "Blocker_<anything>" nodes (Blender cube empties,
    //           display size 1, scaled and rotated to cover the area), e.g. level bounds. They
    //           block players, NPCs and rays but are never drawn. Destroy them with the level.
    struct SceneLevel
    {
        lockstep::assets::Scene scene;
        lockstep::assets::SceneCollision collision;
        std::vector<lockstep::physics::BodyId> blockers;
    };

    // Where players and the enemy appear in a level.
    // pawns: foot positions of Player1's and Player2's pawns.
    // enemy: foot position of the enemy.
    struct LevelSpawns
    {
        std::array<math::Vec3, k_playerCount> pawns{};
        math::Vec3 enemy{0.0f, 0.0f, 0.0f};
    };

    // Reads `--level`, `--scene` and `--scene-scale` from the command line.
    // arguments: command-line options.
    // Returns:   the level to load, nothing without `--level` or `--scene`, or an error for a
    //            missing value or a scale that is not a positive number.
    [[nodiscard]] lockstep::Result<std::optional<SceneLaunch>> parseSceneLaunch(std::span<const char* const> arguments);

    // Loads the cooked level or imports the glTF scene, builds its colliders, applies its look
    // (interior ambient zones, decal instances, time-of-day preset; see LevelAmbience.hpp and
    // LevelLook.hpp) and logs load time, triangles, texture memory and import warnings.
    // launch:   the level or file, and scale.
    // renderer: uploads the scene.
    // physics:  receives the colliders.
    // files:    the mounted content a cooked level is read from.
    // Returns:  the level, or the load or collision error.
    [[nodiscard]] lockstep::Result<SceneLevel> loadSceneLevel(const SceneLaunch& launch,
        lockstep::render::Renderer& renderer, lockstep::physics::PhysicsWorld& physics,
        const lockstep::assets::FileSystem& files);

    // Finds spawn points: Player1 and Player2 at "PlayerStart_1" and "PlayerStart_2" when the scene
    // has them; otherwise beside a node named "PlayerStart" or "Spawn" if the scene has one, else on
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
