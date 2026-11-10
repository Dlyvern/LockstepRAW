#ifndef LOCKSTEP_GAME_WEAPONS_WEAPON_SOCKETS_HPP
#define LOCKSTEP_GAME_WEAPONS_WEAPON_SOCKETS_HPP

#include "Weapons/WeaponKind.hpp"

#include <Lockstep/Anim/SkinnedModel.hpp>
#include <Lockstep/Core/Math.hpp>
#include <Lockstep/Core/Transform.hpp>
#include <Lockstep/World/Entities.hpp>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>

namespace game
{
    namespace math = lockstep::math;

    struct FirstPersonModel;
    struct NpcLooks;
    struct PawnCharacters;
    struct ViewModelState;

    // Points on a held KRISS in world space, where they appear on screen (for the first-person
    // weapon, moved to where the world camera shows them; see ViewModelSockets).
    // muzzle:    the end of the barrel.
    // forward:   unit direction the barrel points in.
    // up:        unit direction towards the top of the weapon.
    // right:     unit direction to the weapon's right (the ejection side).
    // port:      the ejection port on the right of the receiver.
    // magazine:  the magazine's centre and orientation (its +y points up the magazine, into the
    //            magazine well; +x is its thin side).
    // optic:     the optic rail socket: a point on top of the rail where an optic's mount sits
    //            (the mount's rail contact), its -z along the rail towards the muzzle and +y up.
    struct WeaponSocket
    {
        math::Vec3 muzzle{0.0f, 0.0f, 0.0f};
        math::Vec3 forward{0.0f, 0.0f, -1.0f};
        math::Vec3 up{0.0f, 1.0f, 0.0f};
        math::Vec3 right{1.0f, 0.0f, 0.0f};
        math::Vec3 port{0.0f, 0.0f, 0.0f};
        math::Transform magazine{};
        math::Transform optic{};
    };

    // Points on a weapon in the space of the joint that carries it (directions are unit vectors).
    // joint:         the carrying joint (matched by name after any "prefix:"), e.g. the right hand for
    //                the KRISS (skinned to the hand) or W_Main for the M4 and Glock rigs.
    // magazineJoint: the joint that moves the magazine in the reload clips, or empty for none.
    // muzzle, forward, up: the barrel's end, its direction and the weapon's up.
    // port:          the ejection port.
    // magazine, magazineAxis: the magazine's centre and its axis up into the well.
    // optic, opticForward, opticUp: the optic rail mount and its axes.
    struct WeaponPoints
    {
        std::string_view joint;
        std::string_view magazineJoint;
        math::Vec3 muzzle{0.0f, 0.0f, 0.0f};
        math::Vec3 forward{0.0f, 0.0f, -1.0f};
        math::Vec3 up{0.0f, 1.0f, 0.0f};
        math::Vec3 port{0.0f, 0.0f, 0.0f};
        math::Vec3 magazine{0.0f, 0.0f, 0.0f};
        math::Vec3 magazineAxis{0.0f, 1.0f, 0.0f};
        math::Vec3 optic{0.0f, 0.0f, 0.0f};
        math::Vec3 opticForward{0.0f, 0.0f, -1.0f};
        math::Vec3 opticUp{0.0f, 1.0f, 0.0f};
    };

    // The points of a weapon on its first-person rig: the KRISS' in right-hand space (measured from
    // swat_armed.glb by Content/source/scripts/kriss_sockets.py; the armed body holds it the same
    // way), the M4's and Glock's in W_Main joint space (measured by fp_weapon.py).
    // weapon:  a loadout weapon.
    // Returns: its points.
    [[nodiscard]] const WeaponPoints& firstPersonWeaponPoints(WeaponKind weapon);

    // Moves weapon points into the space of the joint carrying a held gun, e.g. W_Main points into
    // right-hand space by the held gun's W_Main node.
    // points:  the points.
    // carrier: where their joint sits in the new joint's space.
    // Returns: the points in the new joint's space (joint: the right hand, no magazine joint).
    [[nodiscard]] WeaponPoints heldWeaponPoints(const WeaponPoints& points, const math::Transform& carrier);

    // The sockets of a weapon a skinned model carries, in world space.
    // model:     the character model.
    // instance:  its current pose (updated this frame).
    // placement: the model's world placement.
    // points:    the weapon's points on that model.
    // Returns:   the sockets, or nothing if the pose has no such joint.
    [[nodiscard]] std::optional<WeaponSocket> heldWeaponSocket(const lockstep::anim::SkinnedModel& model,
        const lockstep::anim::SkinnedInstance& instance, const math::Transform& placement, const WeaponPoints& points);

    // The optic rail socket of a model holding the weapon, in the model's own space.
    // model:    the character (or first-person) model.
    // instance: its pose.
    // points:   the held weapon's points (firstPersonWeaponPoints()).
    // Returns:  WeaponSocket::optic in model space, or nothing if the model lacks the weapon's joint.
    [[nodiscard]] std::optional<math::Transform> modelOpticSocket(const lockstep::anim::SkinnedModel& model,
        const lockstep::anim::SkinnedInstance& instance, const WeaponPoints& points);

    // The sockets of an armed player pawn as drawn this frame (its KRISS, or its held M4 or Glock).
    // characters: the pawn characters.
    // index:      the player (0 or 1).
    // Returns:    the sockets, or nothing if the pawn was not drawn armed this frame.
    [[nodiscard]] std::optional<WeaponSocket> pawnWeaponSocket(const PawnCharacters& characters, std::size_t index);

    // The sockets of an armed NPC as drawn this frame.
    // looks:   the NPC looks.
    // entity:  the NPC's local entity.
    // Returns: the sockets, or nothing if it was not drawn this frame or dropped its weapon.
    [[nodiscard]] std::optional<WeaponSocket> npcWeaponSocket(const NpcLooks& looks, lockstep::world::EntityId entity);

    // The sockets of the local first-person weapon as drawn this frame (drawViewModel()), following
    // the hands through the reload clips (the points of FirstPersonModel::weapon; the magazine
    // follows its magazine joint when the model has one).
    // model:    the first-person model.
    // state:    the viewmodel state.
    // worldFov: the world camera's vertical field of view, radians.
    // Returns:  the sockets, or nothing while the viewmodel is hidden.
    [[nodiscard]] std::optional<WeaponSocket> viewModelWeaponSocket(const FirstPersonModel& model,
        const ViewModelState& state, float worldFov);

    // The sockets of the local first-person weapon in the space its draws on k_viewModelLayer are
    // placed in (the viewmodel's own field of view and squeezed depth, not moved to the world
    // camera), for things drawn on that layer with the weapon, such as the muzzle flash.
    // model:   the first-person model.
    // state:   the viewmodel state.
    // Returns: the sockets, or nothing while the viewmodel is hidden.
    [[nodiscard]] std::optional<WeaponSocket> viewModelLayerSocket(const FirstPersonModel& model,
        const ViewModelState& state);
} //namespace game

#endif //LOCKSTEP_GAME_WEAPONS_WEAPON_SOCKETS_HPP
