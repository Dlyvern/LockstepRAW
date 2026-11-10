#ifndef LOCKSTEP_GAME_WEAPONS_WEAPON_DEBRIS_HPP
#define LOCKSTEP_GAME_WEAPONS_WEAPON_DEBRIS_HPP

#include "Audio/Surfaces.hpp"

#include <Lockstep/Core/Math.hpp>
#include <Lockstep/Core/Result.hpp>
#include <Lockstep/Core/Transform.hpp>
#include <Lockstep/Physics/PhysicsTypes.hpp>
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
    struct SceneLevel;

    // How one kind of small weapon debris (spent casings, dropped magazines) moves and rests. It is
    // simulated cheaply on this machine: gravity, one raycast per moving piece per frame, bounces
    // with restitution and friction, then it lies on its side (its local x axis along the surface
    // normal) until it expires.
    // maxPieces:    pieces kept (the oldest is reused).
    // lifeSeconds:  World seconds a piece stays.
    // halfThickness: distance from the centre to the surface when lying (half the local x extent).
    // restitution:  share of the speed into a surface it bounces back with.
    // friction:     share of the speed along a surface it keeps on a bounce.
    // spinKept:     share of its spin kept on a bounce.
    // spinKick:     spin added per m/s of impact speed, rad/s.
    // restSpeed:    below this speed after a bounce on a floor it comes to rest, m/s.
    // soundSpeed:   bounces faster than this into the surface make a sound, m/s.
    // fullSoundSpeed: impact speed of full volume, m/s.
    // maxSoundBounces: bounces of one piece that make a sound.
    struct DebrisKind
    {
        uint32_t maxPieces{150};
        float lifeSeconds{30.0f};
        float halfThickness{0.006f};
        float restitution{0.4f};
        float friction{0.6f};
        float spinKept{0.6f};
        float spinKick{6.0f};
        float restSpeed{0.35f};
        float soundSpeed{0.5f};
        float fullSoundSpeed{3.5f};
        uint32_t maxSoundBounces{3};
    };

    // One piece.
    // position, velocity: world, m/s.
    // rotation, spin:     orientation and angular velocity (world, rad/s).
    // rest:               the orientation it settles into once resting.
    // age:                World seconds since it was spawned.
    // settle:             0 at the moment it came to rest, 1 settled.
    // bounces:            surfaces hit so far.
    // isResting:          lying still.
    // ignore:             a character controller its rays ignore (its shooter's).
    struct DebrisPiece
    {
        math::Vec3 position{0.0f, 0.0f, 0.0f};
        math::Vec3 velocity{0.0f, 0.0f, 0.0f};
        math::Quat rotation{};
        math::Vec3 spin{0.0f, 0.0f, 0.0f};
        math::Quat rest{};
        float age{0.0f};
        float settle{0.0f};
        uint32_t bounces{0};
        bool isResting{false};
        lockstep::physics::ControllerId ignore{};
    };

    // A piece hitting a surface hard enough to be heard.
    // position: where.
    // surface:  what it hit.
    // loudness: 0 to 1 by impact speed.
    struct DebrisLanding
    {
        math::Vec3 position{0.0f, 0.0f, 0.0f};
        Surface surface{Surface::Concrete};
        float loudness{1.0f};
    };

    // The pieces of one kind and what it is drawn with.
    // kind:     how they behave.
    // mesh, material: the look (created by the caller).
    // pieces:   fixed slots, so the draw order stays stable for motion vectors.
    // next:     the slot the next spawn reuses once all are taken.
    // landings: the audible landings of the last update.
    struct DebrisPool
    {
        DebrisKind kind{};
        lockstep::render::MeshHandle mesh{};
        lockstep::render::MaterialHandle material{};
        std::vector<DebrisPiece> pieces{};
        std::size_t next{0};
        std::vector<DebrisLanding> landings{};
    };

    // Adds a piece, reusing the oldest slot when the pool is full.
    // pool:  the pool.
    // piece: its start (position, velocity, rotation, spin, ignored controller).
    void spawnDebris(DebrisPool& pool, const DebrisPiece& piece);

    // Moves, bounces, settles and expires the pieces; fills pool.landings.
    // pool:    the pool.
    // physics: the world the rays are cast in.
    // level:   the loaded level (surface names), or null.
    // seconds: World seconds since the previous frame.
    void updateDebris(DebrisPool& pool, const lockstep::physics::PhysicsWorld& physics, const SceneLevel* level,
        float seconds);

    // Queues the pieces for drawing (they shrink away in their last half second).
    // pool:     the pool.
    // renderer: the renderer.
    void drawDebris(const DebrisPool& pool, lockstep::render::Renderer& renderer);

    // Removes every piece (level switch).
    // pool: the pool.
    void clearDebris(DebrisPool& pool);

    // Creates a .45 ACP casing: a brass cylinder (11.5 mm across, 22.8 mm long) along local y.
    // renderer: the renderer.
    // pool:     receives the mesh and material.
    // Returns:  an error if the mesh or material could not be created.
    [[nodiscard]] lockstep::Status createCasingLook(lockstep::render::Renderer& renderer, DebrisPool& pool);

    // Creates a KRISS magazine: a dark polymer box (30 x 245 x 45 mm, long along local y, thin
    // along local x).
    // renderer: the renderer.
    // pool:     receives the mesh and material.
    // Returns:  an error if the mesh or material could not be created.
    [[nodiscard]] lockstep::Status createMagazineLook(lockstep::render::Renderer& renderer, DebrisPool& pool);
} //namespace game

#endif //LOCKSTEP_GAME_WEAPONS_WEAPON_DEBRIS_HPP
