#ifndef LOCKSTEP_GAME_CHARACTERS_NPC_BODIES_HPP
#define LOCKSTEP_GAME_CHARACTERS_NPC_BODIES_HPP

#include "Characters/CharacterModel.hpp"
#include "Characters/GearLooks.hpp"
#include "Characters/Locomotion.hpp"

#include <array>
#include <optional>
#include <string_view>

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
    struct PawnCharacters;

    // Cooked scenes of one NPC body (see NpcBody). They hold a skinned model named like the pawn
    // model ("SWAT") with the SWAT skeleton and clips, cooked with --limbs. Empty: not loaded
    // (the SWAT body uses the pawn models).
    // unarmed: virtual path of the unarmed body.
    // armed:   virtual path of the body holding the weapon.
    struct NpcBodyScenes
    {
        std::string_view unarmed;
        std::string_view armed;
    };

    // Scenes of every NpcBody, indexed by it. The robber is only ever armed.
    inline constexpr std::array<NpcBodyScenes, k_npcBodyCount> k_npcBodyScenes{{
        {.unarmed = "", .armed = ""},
        {.unarmed = "characters/tactical/scene", .armed = "characters/tactical_armed/scene"},
        {.unarmed = "", .armed = "characters/robber_armed/scene"}}};

    // An armed NPC body and its two locomotion sets.
    struct NpcArmedBody
    {
        CharacterModel body;
        LocomotionSet rifle{};
        LocomotionSet aiming{};
    };

    // One NPC body's loaded models; either may be missing (not mounted, or never used).
    struct NpcBodyModels
    {
        std::optional<CharacterModel> unarmed{};
        std::optional<NpcArmedBody> armed{};
    };

    // The NPC bodies other than the SWAT pawn models, indexed by NpcBody (the Swat entry stays
    // empty).
    struct NpcBodies
    {
        std::array<NpcBodyModels, k_npcBodyCount> bodies{};
    };

    // An armed body to draw an NPC with: views into loaded models.
    // body:   the model and its gear masks.
    // rifle:  relaxed rifle locomotion.
    // aiming: weapon-up locomotion.
    struct NpcArmedView
    {
        const CharacterModel* body{nullptr};
        const LocomotionSet* rifle{nullptr};
        const LocomotionSet* aiming{nullptr};
    };

    // Loads the k_npcBodyScenes models that are mounted; missing ones are logged and left out
    // (those NPCs are then drawn with the SWAT model).
    // renderer: uploads the models.
    // files:    the mounted content.
    // rifle:    clip names of the relaxed rifle locomotion.
    // aiming:   clip names of the weapon-up locomotion.
    // Returns:  the bodies found.
    [[nodiscard]] NpcBodies loadNpcBodies(lockstep::render::Renderer& renderer,
        const lockstep::assets::FileSystem& files, const LocomotionClipNames& rifle,
        const LocomotionClipNames& aiming);

    // characters: the pawn characters (SWAT models and the NPC bodies).
    // body:       the body wanted.
    // Returns:    its unarmed model, or the SWAT unarmed model when it isn't loaded.
    [[nodiscard]] const CharacterModel& npcUnarmedModel(const PawnCharacters& characters, NpcBody body);

    // characters: the pawn characters.
    // body:       the body wanted.
    // Returns:    its armed model and clips, or the SWAT armed model's when it isn't loaded, or
    //             nothing without an armed SWAT model either.
    [[nodiscard]] std::optional<NpcArmedView> npcArmedModel(const PawnCharacters& characters, NpcBody body);
} //namespace game

#endif //LOCKSTEP_GAME_CHARACTERS_NPC_BODIES_HPP
