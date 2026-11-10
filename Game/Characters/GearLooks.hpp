#ifndef LOCKSTEP_GAME_CHARACTERS_GEAR_LOOKS_HPP
#define LOCKSTEP_GAME_CHARACTERS_GEAR_LOOKS_HPP

#include <Lockstep/Anim/SkinnedModel.hpp>
#include <Lockstep/World/Entities.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>

namespace game
{
    // Which head gear a character of the SWAT model wears, so people in the same model differ.
    // Presentation only; every machine derives it from the same data (player index, marker name).
    // Full:      everything (helmet, night-vision goggles, headset).
    // NoNvg:     without the night-vision goggles.
    // Balaclava: only the balaclava (no helmet, goggles or headset).
    enum class GearLook : uint8_t
    {
        Full = 0,
        NoNvg,
        Balaclava
    };

    inline constexpr std::size_t k_gearLookCount{3};

    // Names of the head gear materials of the character models; each is its own part.
    // nvg:     night-vision goggles and mount.
    // helmet:  helmet shell.
    // headset: ear cups, mic boom, chin strap and cables.
    struct GearMaterialNames
    {
        std::string_view nvg;
        std::string_view helmet;
        std::string_view headset;
    };

    inline constexpr GearMaterialNames k_gearMaterials{.nvg = "HeadGear_NVG",
        .helmet = "HeadGear_Helmet",
        .headset = "HeadGear_Headset"};

    // The parts of one model to draw for each GearLook (indexed by the look).
    using GearMasks = std::array<lockstep::anim::PartMask, k_gearLookCount>;

    // Finds the head gear parts of a model by material name. Parts it can't find are simply
    // never hidden (a model without them draws the same in every look). Wound caps of a model
    // cooked with limbs are left out of every look.
    // model:   the character model.
    // Returns: the part mask of every look.
    [[nodiscard]] GearMasks findGearMasks(const lockstep::anim::SkinnedModel& model);

    // masks:   a model's masks.
    // look:    the look.
    // Returns: the parts to draw for that look.
    [[nodiscard]] lockstep::anim::PartMask gearMask(const GearMasks& masks, GearLook look);

    // Reads a look override from a marker name: a part "_Full", "_NoNVG" or "_Balaclava" (any
    // case; a Blender ".001" ending is ignored), e.g. Npc_SquadOutside_2_Balaclava.
    // name:    the marker's node name.
    // Returns: the look, or nothing without such a part.
    [[nodiscard]] std::optional<GearLook> gearLookSuffix(std::string_view name);

    // owner:   a player.
    // Returns: that player's look: Player1 Full, Player2 NoNvg (the same on both machines).
    [[nodiscard]] GearLook playerGearLook(lockstep::world::EntityOwner owner);

    // The look of a squad officer or police placeholder: the name's override, else a mix picked
    // by the marker's number (its last digits, or a hash of the name without any).
    // name:    the marker's node name.
    // Returns: the look.
    [[nodiscard]] GearLook squadGearLook(std::string_view name);

    // The look of a merc: the name's override, else mostly Balaclava with some NoNvg, picked by
    // the marker's number.
    // name:    the spawn marker's node name.
    // Returns: the look.
    [[nodiscard]] GearLook mercGearLook(std::string_view name);

    // Which character model a person is drawn with (all share the SWAT skeleton and clips).
    // Presentation only; derived from the marker name like the GearLook, so both machines agree.
    // Swat:     the SWAT operator (the players' model).
    // Tactical: the tactical outfit, the other squad and police model.
    // Robber:   the masked robber the mercs wear.
    enum class NpcBody : uint8_t
    {
        Swat = 0,
        Tactical,
        Robber
    };

    inline constexpr std::size_t k_npcBodyCount{3};

    // The body of a squad officer or police placeholder: a name part "_Swat" or "_Tactical" (any
    // case), else the SWAT operator or the tactical outfit alternating by the marker's number (a
    // hash of the name without one).
    // name:    the marker's node name.
    // Returns: the body.
    [[nodiscard]] NpcBody squadBody(std::string_view name);

    // Returns: the body every merc is drawn with (Robber).
    [[nodiscard]] NpcBody mercBody();
} //namespace game

#endif //LOCKSTEP_GAME_CHARACTERS_GEAR_LOOKS_HPP
