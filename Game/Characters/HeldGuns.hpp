#ifndef LOCKSTEP_GAME_CHARACTERS_HELD_GUNS_HPP
#define LOCKSTEP_GAME_CHARACTERS_HELD_GUNS_HPP

#include "Weapons/WeaponKind.hpp"

#include <Lockstep/Assets/Scene.hpp>
#include <Lockstep/Core/Math.hpp>
#include <Lockstep/Core/Transform.hpp>
#include <Lockstep/Render/Views.hpp>

#include <optional>

namespace lockstep::assets
{
    class FileSystem;
} //namespace lockstep::assets

namespace lockstep::render
{
    class Renderer;
} //namespace lockstep::render

namespace game
{
    namespace math = lockstep::math;

    // A gun a third-person body holds in its right hand instead of the KRISS of the armed model:
    // a static scene (weapons/<name>/scene, exported by Content/source/scripts/fp_weapon.py) whose
    // meshes sit in the armed body's right-hand joint space, so it is drawn at that joint.
    // scene: the meshes (in right-hand space).
    // main:  where the first-person rig's W_Main joint (the space of the weapon's socket points)
    //        sits in right-hand space, found from the scene's "Muzzle" node (-z along the barrel).
    struct HeldGun
    {
        lockstep::assets::Scene scene;
        math::Transform main{};
    };

    // Loads the held gun of a loadout weapon.
    // renderer: uploads the meshes.
    // files:    the mounted content.
    // weapon:   the M4 or the Glock (the KRISS is part of the armed body).
    // muzzleLocal: the muzzle in W_Main space (firstPersonWeaponPoints()).
    // Returns:  the gun, or nothing when it is not mounted or fails to load (logged).
    [[nodiscard]] std::optional<HeldGun> loadHeldGun(lockstep::render::Renderer& renderer,
        const lockstep::assets::FileSystem& files, WeaponKind weapon, const math::Vec3& muzzleLocal);

    // Queues a held gun's meshes at a hand.
    // renderer: the renderer.
    // gun:      the gun.
    // hand:     the right-hand joint's world transform.
    // layers:   the render layers to draw on.
    void drawHeldGun(lockstep::render::Renderer& renderer, const HeldGun& gun, const math::Mat4& hand,
        lockstep::render::RenderLayers layers);
} //namespace game

#endif //LOCKSTEP_GAME_CHARACTERS_HELD_GUNS_HPP
