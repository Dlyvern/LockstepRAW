#ifndef LOCKSTEP_GAME_GORE_BLOOD_DECALS_HPP
#define LOCKSTEP_GAME_GORE_BLOOD_DECALS_HPP

#include <Lockstep/Core/Math.hpp>
#include <Lockstep/Physics/PhysicsTypes.hpp>
#include <Lockstep/Render/Resources.hpp>

#include <cstdint>
#include <span>
#include <vector>

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

    // One blood decal lying on a surface (presentation only).
    // material:    its blood material (splatter, pool or drop).
    // position:    the surface point, already lifted a few millimetres off it.
    // rotation:    takes the decal quad's +Y onto the surface normal, spun around it.
    // size:        full edge length, metres.
    // stretch:     length over width along the quad's x (1 round, more for smears).
    // growSeconds: World seconds it takes to reach full size (pools); 0 for at once.
    // age:         World seconds since it was laid.
    // runSeconds:  for a run down a wall, World seconds it takes to flow its full length (its
    //              position is then the top of the run and its quad +Z points down the wall);
    //              0 for an ordinary decal.
    struct BloodDecal
    {
        lockstep::render::MaterialHandle material{};
        math::Vec3 position{0.0f, 0.0f, 0.0f};
        math::Quat rotation{};
        float size{0.5f};
        float stretch{1.0f};
        float growSeconds{0.0f};
        float age{0.0f};
        float runSeconds{0.0f};
    };

    // Every blood decal of the level, oldest first (later ones draw over earlier ones).
    // decals:      the decals.
    // unpaintable: bodies blood never lands on (the level's invisible blockers).
    struct BloodDecals
    {
        std::vector<BloodDecal> decals;
        std::vector<lockstep::physics::BodyId> unpaintable;
    };

    // decals:  the decals (their unpaintable bodies).
    // hit:     a raycast hit.
    // Returns: true if blood may land there: level geometry that is drawn.
    [[nodiscard]] bool isPaintable(const BloodDecals& decals, const lockstep::physics::RaycastHit& hit);

    // Returns the rotation turning one unit direction onto another (the shortest arc).
    // from, to: unit directions.
    // Returns:  the rotation.
    [[nodiscard]] math::Quat rotationBetween(const math::Vec3& from, const math::Vec3& to);

    // Builds the placement of a decal on a surface.
    // point:  the surface point.
    // normal: the unit surface normal.
    // spin:   rotation around the normal, radians.
    // Returns: a decal at `point` lifted off the surface, with its rotation set (material and
    //          size still to fill in).
    [[nodiscard]] BloodDecal decalOnSurface(const math::Vec3& point, const math::Vec3& normal, float spin);

    // normal:  a unit surface normal.
    // Returns: true if blood runs down the surface (a wall or a steep slope).
    [[nodiscard]] bool isRunningSurface(const math::Vec3& normal);

    // Lays a run of blood flowing down a wall from an impact: it starts at `point` and is revealed
    // downwards over `seconds` (World time, so bullet-time slows it), then stays. Skipped when the
    // wall below is too small for it.
    // decals:    the decals.
    // physics:   the physics world, to fit it onto the wall.
    // material:  a run material (impact at the texture's top, runs below).
    // point:     the impact point on the wall.
    // normal:    the wall's unit normal (isRunningSurface()).
    // length:    full length of the run, metres (its width is a quarter of it).
    // seconds:   World seconds it takes to flow.
    // maxDecals: most decals kept.
    void addWallRun(BloodDecals& decals, const lockstep::physics::PhysicsWorld& physics,
        lockstep::render::MaterialHandle material, const math::Vec3& point, const math::Vec3& normal, float length,
        float seconds, uint32_t maxDecals);

    // Fits a decal onto the surface it was placed on, so it never hangs off the edge of a small
    // object (a chair, a crate, a bed frame): rays around its footprint must all hit level
    // geometry at about the same depth with about the same normal; otherwise it is shrunk and
    // tried again.
    // physics: the physics world, for raycasts (debris is not hit).
    // decal:   the decal, placed by decalOnSurface() with its size and stretch set; its size may
    //          shrink.
    // minSize: smallest size worth keeping, metres.
    // Returns: true if it fits (at its possibly smaller size), false to drop it.
    [[nodiscard]] bool fitDecal(const lockstep::physics::PhysicsWorld& physics, BloodDecal& decal, float minSize);

    // Adds a decal, dropping the oldest small drip past the limit (the oldest decal of any kind
    // when there is none), so pools and splatters outlast drips.
    // decals:    the decals.
    // decal:     the new decal.
    // maxDecals: most decals kept.
    void addDecal(BloodDecals& decals, const BloodDecal& decal, uint32_t maxDecals);

    // Ages the decals (pools grow).
    // decals:  the decals.
    // seconds: World seconds since the last frame.
    void updateDecals(BloodDecals& decals, float seconds);

    // Queues every decal for the next render() with Renderer::submitDecal(); a run shows as much
    // of itself as has flowed so far.
    // renderer: the renderer.
    // decals:   the decals.
    // quad:     a unit quad in the XZ plane facing +Y.
    // runQuads: the quad's top parts in growing steps (BloodAssets::runQuads); empty skips runs.
    void drawDecals(lockstep::render::Renderer& renderer, const BloodDecals& decals, lockstep::render::MeshHandle quad,
        std::span<const lockstep::render::MeshHandle> runQuads);
} //namespace game

#endif //LOCKSTEP_GAME_GORE_BLOOD_DECALS_HPP
