#ifndef LOCKSTEP_GAME_GORE_BLOOD_ASSETS_HPP
#define LOCKSTEP_GAME_GORE_BLOOD_ASSETS_HPP

#include <Lockstep/Assets/Scene.hpp>
#include "Gore/BloodDecals.hpp"
#include "Gore/GoreRandom.hpp"

#include <Lockstep/Core/Result.hpp>
#include <Lockstep/Physics/PhysicsTypes.hpp>
#include <Lockstep/Render/Resources.hpp>

#include <array>
#include <cstddef>
#include <optional>

namespace lockstep::assets
{
    class FileSystem;
} //namespace lockstep::assets

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
    // Number of splatter looks (each its own generated texture).
    inline constexpr std::size_t k_splatterLooks{3};

    // Number of wall run looks (blood running down a wall from an impact).
    inline constexpr std::size_t k_runLooks{3};

    // Number of steps a wall run is revealed in (one quad mesh each, see runQuads).
    inline constexpr std::size_t k_runSteps{16};

    // The GPU resources blood is drawn with: the decal quad, the droplet mesh, and decal
    // materials: the blood art of the cooked gore scene (characters/gore/scene; nodes Blood_Splat
    // and Blood_Splatter for splatters, Blood_Pool for pools, Blood_Run_1..3 for wall runs; from
    // Content/source/characters/gore) when it is mounted, else procedurally generated textures
    // (see bloodTexture()) and no runs; drips are always generated. Owned by the Renderer for the
    // whole game.
    // art:        the gore scene, kept loaded while its materials are used.
    // decalQuad:  unit quad in the XZ plane facing +Y, for submitDecal().
    // droplet:    unit cube for flying droplets.
    // splatters:  wet, alpha-blended splatter materials.
    // runs:       wall run materials: an impact at the top, runs below (texture top = run start).
    // runQuads:   the decal quad's top 1/k_runSteps, 2/k_runSteps, ..., all of it (the texture
    //             cut the same way), to reveal a run as it flows down.
    // hasRuns:    the run materials exist.
    // pool:       the pool material.
    // drop:       the small drip material.
    // dropletLook: glossy dark red, opaque, for flying droplets.
    struct BloodAssets
    {
        lockstep::render::MeshHandle decalQuad{};
        lockstep::render::MeshHandle droplet{};
        std::array<lockstep::render::MaterialHandle, k_splatterLooks> splatters{};
        std::array<lockstep::render::MaterialHandle, k_runLooks> runs{};
        std::array<lockstep::render::MeshHandle, k_runSteps> runQuads{};
        bool hasRuns{false};
        lockstep::render::MaterialHandle pool{};
        lockstep::render::MaterialHandle drop{};
        lockstep::render::MaterialHandle dropletLook{};
        std::optional<lockstep::assets::Scene> art{};
    };

    // Loads the blood art (or generates stand-ins) and creates the materials and meshes.
    // renderer: the renderer.
    // files:    the mounted content (the gore scene is in characters.pak).
    // Returns:  the assets, or the first creation error.
    [[nodiscard]] lockstep::Result<BloodAssets> createBloodAssets(lockstep::render::Renderer& renderer,
        const lockstep::assets::FileSystem& files);

    // How often and how long blood runs down a wall under a splatter.
    // chance:     share of wall splatters that run.
    // minSeconds, maxSeconds: World seconds a run takes to flow.
    // lengthShare: run length over the splatter's size.
    struct WallRunTuning
    {
        float chance{0.65f};
        float minSeconds{3.0f};
        float maxSeconds{7.0f};
        float lengthShare{0.9f};
    };

    // Lets a splatter on a wall run down it, some of the time (see addWallRun()). Nothing
    // happens on floors, ceilings or without run materials.
    // decals:    the decals.
    // assets:    the blood assets (run materials).
    // physics:   the physics world.
    // hit:       the wall point and normal the splatter landed on.
    // size:      the splatter's size, metres.
    // tuning:    how often and how long.
    // maxDecals: most decals kept.
    // random:    picks the look, length and speed.
    void layWallRun(BloodDecals& decals, const BloodAssets& assets, const lockstep::physics::PhysicsWorld& physics,
        const lockstep::physics::RaycastHit& hit, float size, const WallRunTuning& tuning, uint32_t maxDecals,
        GoreRandom& random);
} //namespace game

#endif //LOCKSTEP_GAME_GORE_BLOOD_ASSETS_HPP
