#ifndef LOCKSTEP_GAME_WEAPONS_WEAPON_SOCKETS_HPP
#define LOCKSTEP_GAME_WEAPONS_WEAPON_SOCKETS_HPP

#include <Lockstep/Anim/SkinnedModel.hpp>
#include <Lockstep/Core/Math.hpp>
#include <Lockstep/Core/Transform.hpp>
#include <Lockstep/World/Entities.hpp>

#include <cstddef>
#include <cstdint>
#include <optional>

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
    struct WeaponSocket
    {
        math::Vec3 muzzle{0.0f, 0.0f, 0.0f};
        math::Vec3 forward{0.0f, 0.0f, -1.0f};
        math::Vec3 up{0.0f, 1.0f, 0.0f};
        math::Vec3 right{1.0f, 0.0f, 0.0f};
        math::Vec3 port{0.0f, 0.0f, 0.0f};
        math::Transform magazine{};
    };

    // The KRISS sockets of a character holding it in its right hand (the armed SWAT body, whose
    // weapon is skinned to the right hand only; the offsets were measured from
    // Content/source/characters/swat/swat_armed.glb by Content/source/scripts/kriss_sockets.py).
    // model:     the character model.
    // instance:  its current pose (updated this frame).
    // placement: the model's world placement.
    // hand:      the right hand joint (HeldWeapon::hand).
    // Returns:   the sockets, or nothing if the pose has no such joint.
    [[nodiscard]] std::optional<WeaponSocket> heldWeaponSocket(const lockstep::anim::SkinnedModel& model,
        const lockstep::anim::SkinnedInstance& instance, const math::Transform& placement, uint32_t hand);

    // The sockets of an armed player pawn as drawn this frame.
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
    // the hands through the reload clips; the magazine follows the KRISS_Mag joint when the model has
    // one.
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
