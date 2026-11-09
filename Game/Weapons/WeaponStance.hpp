#ifndef LOCKSTEP_GAME_WEAPONS_WEAPON_STANCE_HPP
#define LOCKSTEP_GAME_WEAPONS_WEAPON_STANCE_HPP

#include <Lockstep/Core/Math.hpp>
#include <Lockstep/Core/Transform.hpp>

#include <cstdint>
#include <span>
#include <vector>

namespace lockstep::assets
{
    struct Scene;
} //namespace lockstep::assets

namespace game
{
    namespace math = lockstep::math;

    // The weapon rule of a place. An armed player's weapon is up (ready) by default and may be
    // lowered with the lower key; a zone overrides that.
    // Open:    up by default, lowering allowed.
    // Safe:    always lowered; no firing or aiming down sights. A running combat situation turns
    //          safe zones off (see effectiveWeaponZone()).
    // Hostile: always up; the weapon can't be lowered.
    enum class WeaponZone : uint8_t
    {
        Open = 0,
        Safe,
        Hostile
    };

    // A weapon zone authored in the level: a glTF node named "Zone_Safe_*" or "Zone_Hostile_*"
    // (an empty or a mesh; only its transform is read). The zone is the node's local box
    // [-1, 1]^3 placed by its world transform, so a Blender cube empty with display size 1 (or the
    // default 2 m cube mesh) covers exactly the zone; scale and rotate it to fit.
    // box:  the node's world transform (position = centre, scale = half extents, rotation).
    // kind: Safe or Hostile.
    struct WeaponZoneBox
    {
        math::Transform box{};
        WeaponZone kind{WeaponZone::Open};
    };

    // Finds the weapon zones of a level and logs how many there are.
    // scene:   the loaded level.
    // Returns: its zones (none when the level has no zone nodes).
    [[nodiscard]] std::vector<WeaponZoneBox> findWeaponZones(const lockstep::assets::Scene& scene);

    // The weapon rule at a point. Where a safe and a hostile zone overlap, Safe wins.
    // zones:    the level's zones.
    // position: a world position (a pawn's position).
    // Returns:  the zone kind there, Open outside every zone.
    [[nodiscard]] WeaponZone weaponZoneAt(std::span<const WeaponZoneBox> zones, const math::Vec3& position);

    // The weapon rule that applies at a place right now: during combat a safe zone acts like an
    // open area, so players can raise their weapons anywhere.
    // zone:           the zone at the place (weaponZoneAt()).
    // isCombatActive: a combat situation is running (host-authoritative, replicated).
    // Returns:        Open for a safe zone during combat, else `zone`.
    [[nodiscard]] WeaponZone effectiveWeaponZone(WeaponZone zone, bool isCombatActive);

    // Whether an armed player's weapon is lowered. Carrying a toy lowers it like a safe zone; once
    // the toy is dropped or thrown the toggle and zone rules apply again.
    // wantsLowered: the player's lower toggle (PlayerCommand::lower).
    // zone:         the zone rule in effect where the pawn stands (effectiveWeaponZone()).
    // isHoldingToy: the player carries a toy (RoomState::heldToy).
    // Returns:      true while holding a toy or in a safe zone, false in a hostile one, else
    //               `wantsLowered`.
    [[nodiscard]] bool isWeaponLowered(bool wantsLowered, WeaponZone zone, bool isHoldingToy);
} //namespace game

#endif //LOCKSTEP_GAME_WEAPONS_WEAPON_STANCE_HPP
