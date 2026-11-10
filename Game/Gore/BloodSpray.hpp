#ifndef LOCKSTEP_GAME_GORE_BLOOD_SPRAY_HPP
#define LOCKSTEP_GAME_GORE_BLOOD_SPRAY_HPP

#include "Gore/BloodDecals.hpp"
#include "Gore/GoreRandom.hpp"

#include <Lockstep/Core/Math.hpp>
#include <Lockstep/Render/Resources.hpp>

#include <cstdint>
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
    // One flying blood droplet: a small ballistic particle that leaves a drip decal where it
    // lands. Presentation only.
    // position, velocity: where it is and how it moves, m and m/s.
    // size:     edge length, metres.
    // seconds:  World seconds left before it disappears unlanded.
    struct Droplet
    {
        math::Vec3 position{0.0f, 0.0f, 0.0f};
        math::Vec3 velocity{0.0f, 0.0f, 0.0f};
        float size{0.01f};
        float seconds{1.5f};
    };

    // Every flying droplet.
    struct BloodSpray
    {
        std::vector<Droplet> droplets;
    };

    // How a burst of droplets leaves a point.
    // point:     where it starts.
    // direction: unit main direction.
    // count:     droplets.
    // speed:     typical speed, m/s.
    // spread:    0 all along `direction` .. 1 every way.
    struct SprayBurst
    {
        math::Vec3 point{0.0f, 0.0f, 0.0f};
        math::Vec3 direction{0.0f, 1.0f, 0.0f};
        uint32_t count{12};
        float speed{3.0f};
        float spread{0.5f};
    };

    // Throws a burst of droplets, dropping the oldest past the limit.
    // spray:        the droplets.
    // burst:        where, which way and how many.
    // maxDroplets:  most droplets kept.
    // random:       picks each droplet's speed, direction and size.
    void throwDroplets(BloodSpray& spray, const SprayBurst& burst, uint32_t maxDroplets, GoreRandom& random);

    // Where droplets that land leave a mark.
    // decals:     the decals to add to.
    // material:   the drip material.
    // maxDecals:  most decals kept.
    // dripSize:   decal size of a droplet of 1 cm, metres.
    struct DripTarget
    {
        BloodDecals& decals;
        lockstep::render::MaterialHandle material{};
        uint32_t maxDecals{0};
        float dripSize{0.08f};
    };

    // Moves the droplets under gravity; one that hits level geometry (characters are passed
    // through) becomes a drip decal there, unless it would hang off a small object (fitDecal()).
    // spray:   the droplets.
    // physics: the physics world, for raycasts.
    // target:  where the drips go.
    // seconds: World seconds since the last frame.
    // random:  spins the drips.
    void updateSpray(BloodSpray& spray, const lockstep::physics::PhysicsWorld& physics, const DripTarget& target,
        float seconds, GoreRandom& random);

    // Queues every droplet for drawing, stretched along its velocity.
    // renderer: the renderer.
    // spray:    the droplets.
    // mesh:     a unit cube.
    // material: the droplet material.
    void drawSpray(lockstep::render::Renderer& renderer, const BloodSpray& spray, lockstep::render::MeshHandle mesh,
        lockstep::render::MaterialHandle material);
} //namespace game

#endif //LOCKSTEP_GAME_GORE_BLOOD_SPRAY_HPP
