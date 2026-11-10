#ifndef LOCKSTEP_GAME_WEAPONS_DROPPED_WEAPONS_HPP
#define LOCKSTEP_GAME_WEAPONS_DROPPED_WEAPONS_HPP

#include "Gore/SeveredLimbs.hpp"

#include <Lockstep/Anim/Limbs.hpp>
#include <Lockstep/Anim/SkinnedModel.hpp>
#include <Lockstep/Core/Result.hpp>
#include <Lockstep/Core/Transform.hpp>

#include <cstdint>
#include <optional>
#include <string_view>
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
    // Material of the weapon the armed character model holds (the KRISS of
    // Content/source/characters/swat/swat_armed.glb, skinned to the right hand only).
    inline constexpr std::string_view k_heldWeaponMaterial{"KSVR"};

    // The weapon an armed character model holds.
    // parts: the model's parts drawn with the weapon material.
    // hand:  the joint the weapon is skinned to (the right hand).
    // limb:  the limb that joint moves (the weapon leaves with it unless dropped first).
    struct HeldWeapon
    {
        lockstep::anim::PartMask parts{0};
        uint32_t hand{lockstep::anim::k_noJoint};
        lockstep::anim::Limb limb{lockstep::anim::Limb::None};
    };

    // Finds the held weapon of a model by its material and the right hand joint ("RightHand",
    // after any "prefix:").
    // model:   a character model.
    // Returns: the weapon, or nothing if the model has no weapon parts or no right hand.
    [[nodiscard]] std::optional<HeldWeapon> findHeldWeapon(const lockstep::anim::SkinnedModel& model);

    // How dropped weapons behave on this machine (dev panel).
    // maxWeapons:  weapons kept lying around (the oldest goes first).
    // lifeSeconds: World seconds a weapon lies around before it is removed.
    // speed:       speed it leaves the hand with along the shot, m/s.
    // lift:        upward speed it leaves with, m/s.
    // spin:        starting spin, rad/s.
    struct WeaponDropTuning
    {
        uint32_t maxWeapons{32};
        float lifeSeconds{300.0f};
        float speed{1.0f};
        float lift{0.6f};
        float spin{4.0f};
    };

    // Weapons dropped by dead characters: the weapon parts frozen in the pose they were dropped
    // in, carried by convex debris bodies. Presentation only; each machine simulates its own.
    // tuning:  the tunables.
    // weapons: the weapons, oldest first.
    struct DroppedWeapons
    {
        WeaponDropTuning tuning{};
        std::vector<SeveredPiece> weapons;
    };

    // The weapon leaving a character's hand.
    // model:     the character model.
    // instance:  its current pose.
    // transform: the model's world placement.
    // weapon:    its held weapon.
    // linear:    starting velocity, m/s.
    // angular:   starting spin, rad/s.
    struct WeaponDrop
    {
        const lockstep::anim::SkinnedModel& model;
        const lockstep::anim::SkinnedInstance& instance;
        const math::Transform& transform;
        const HeldWeapon& weapon;
        math::Vec3 linear{0.0f, 0.0f, 0.0f};
        math::Vec3 angular{0.0f, 0.0f, 0.0f};
    };

    // Drops a held weapon: freezes its parts in the current pose and puts them on a debris body
    // (convex hull of the weapon's shape around the hand), keeping at most maxWeapons. The caller
    // stops drawing the weapon parts on the character.
    // weapons: the dropped weapons.
    // physics: the physics world.
    // drop:    the character and how the weapon leaves.
    // Returns: success, or an error if the body could not be made (nothing is dropped).
    lockstep::Status dropHeldWeapon(DroppedWeapons& weapons, lockstep::physics::PhysicsWorld& physics,
        const WeaponDrop& drop);

    // Moves, expires and queues the dropped weapons for drawing.
    // weapons:  the dropped weapons.
    // physics:  the physics world.
    // renderer: the renderer.
    // alpha:    interpolation between the last two ticks.
    // seconds:  World seconds since the last frame.
    void presentDroppedWeapons(DroppedWeapons& weapons, lockstep::physics::PhysicsWorld& physics,
        lockstep::render::Renderer& renderer, float alpha, float seconds);

    // Removes every dropped weapon (level switch).
    // weapons: the dropped weapons.
    // physics: destroys their bodies.
    void clearDroppedWeapons(DroppedWeapons& weapons, lockstep::physics::PhysicsWorld& physics);
} //namespace game

#endif //LOCKSTEP_GAME_WEAPONS_DROPPED_WEAPONS_HPP
