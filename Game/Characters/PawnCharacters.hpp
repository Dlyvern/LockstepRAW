#ifndef LOCKSTEP_GAME_CHARACTERS_PAWN_CHARACTERS_HPP
#define LOCKSTEP_GAME_CHARACTERS_PAWN_CHARACTERS_HPP

#include "Characters/CharacterModel.hpp"
#include "Characters/Locomotion.hpp"
#include "Characters/ViewModel.hpp"
#include "Player/Pawns.hpp"
#include "Presentation/Visuals.hpp"

#include <Lockstep/Anim/SkinnedModel.hpp>
#include <Lockstep/Core/Transform.hpp>
#include <Lockstep/Render/Views.hpp>
#include <Lockstep/World/Entities.hpp>

#include <array>
#include <cstdint>
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
    // Which character models player pawns are drawn with: the one place to swap them. All three
    // scenes hold a skinned model of this name with the same skeleton and clips.
    // unarmedScene:     virtual path of the unarmed body (mount its pak with --content).
    // armedScene:       the body holding the weapon, drawn while the player is armed.
    // firstPersonScene: the arms and weapon the armed local player sees (the viewmodel).
    // model:            name of the skinned model in those scenes; empty takes the first.
    // unarmed:          locomotion clips of the unarmed body.
    // unarmedCrouch:    its crouched locomotion. There is only a crouched forward walk: walking
    //                   back plays it backwards and strafes reuse it.
    // rifle:            locomotion clips while armed with the weapon lowered. There is no relaxed
    //                   rifle strafe; the aiming strafes are used (the feet match, the weapon rises).
    // aiming:           locomotion clips while armed with the weapon up (ready and aiming down
    //                   sights), with its own idle and turns on the spot.
    // firstPersonAim:   viewmodel pose with the weapon up (ready and aiming down sights).
    struct PawnCharacterChoice
    {
        std::string_view unarmedScene;
        std::string_view armedScene;
        std::string_view firstPersonScene;
        std::string_view model;
        LocomotionClipNames unarmed{};
        LocomotionClipNames unarmedCrouch{};
        LocomotionClipNames rifle{};
        LocomotionClipNames aiming{};
        std::string_view firstPersonAim;
    };

    // Both players use the SWAT placeholder from Content/cooked/characters.pak for now.
    inline constexpr PawnCharacterChoice k_pawnCharacter{.unarmedScene = "characters/swat/scene",
        .armedScene = "characters/swat_armed/scene",
        .firstPersonScene = "characters/swat_fp_kriss/scene",
        .model = "SWAT",
        .unarmedCrouch = {.idle = "CrouchIdle", .walk = "CrouchWalk", .run = "CrouchWalk", .walkBack = "CrouchWalk",
            .strafeLeft = "CrouchWalk", .strafeRight = "CrouchWalk", .turnLeft = "", .turnRight = "",
            .reverseWalkBack = true},
        .rifle = {.idle = "RifleIdle", .walk = "RifleWalk", .run = "RifleRun", .walkBack = "RifleWalkBack",
            .strafeLeft = "AimStrafeLeft", .strafeRight = "AimStrafeRight", .turnLeft = "", .turnRight = ""},
        .aiming = {.idle = "AimIdle", .walk = "AimWalk", .run = "AimRun", .walkBack = "AimWalkBack",
            .strafeLeft = "AimStrafeLeft", .strafeRight = "AimStrafeRight", .turnLeft = "AimTurnLeft",
            .turnRight = "AimTurnRight"},
        .firstPersonAim = "FPAim"};

    // How a pawn is drawn this frame, from replicated (or, for the local player, predicted) state.
    // isArmed:  the player carries the weapon.
    // isAiming: the player aims down sights (only while armed).
    // isLowered: the armed player's weapon is lowered (relaxed rifle set); otherwise it is up
    //            (ready stance, the aiming set).
    // crouch:   how far the pawn is crouched (pawnCrouchAmount(): 0 standing, 1 crouched).
    // isSliding: the pawn slides.
    struct PawnLook
    {
        bool isArmed{false};
        bool isAiming{false};
        bool isLowered{false};
        float crouch{0.0f};
        bool isSliding{false};
    };

    // Stand-in for the crouch and slide clips that are missing (armed crouch, slide): the
    // character is squashed towards its feet and leans back while sliding. Editable from the dev
    // panel.
    // crouchSquash:  fraction of height taken off when fully crouched without crouch clips.
    // crouchClipThreshold: crouch amount from which the unarmed crouch clips play.
    // slideSquash:   extra fraction taken off while sliding.
    // slideLean:     backward lean while sliding, radians.
    // slideBlendRate: how fast the slide pose blends in and out, 1/s.
    struct PawnPoseTuning
    {
        float crouchSquash{0.3f};
        float crouchClipThreshold{0.5f};
        float slideSquash{0.1f};
        float slideLean{0.35f};
        float slideBlendRate{10.0f};
    };

    // One player pawn's animated character. Presentation only, never replicated.
    // instance:      pose and animator.
    // locomotion:    movement observed from where the pawn is drawn.
    // isDrawn:       drawn this frame.
    // wasDrawn:      drawn last frame; a pawn that skipped a frame (dead, absent) restarts tracking.
    // isArmedModel:  drawn with the armed body last time; a switch restarts tracking.
    // slideBlend:    0 upright to 1 in the slide pose, eased.
    struct PawnCharacter
    {
        lockstep::anim::SkinnedInstance instance{};
        LocomotionState locomotion{};
        bool isDrawn{false};
        bool wasDrawn{false};
        bool isArmedModel{false};
        float slideBlend{0.0f};
    };

    // The armed body and its two locomotion sets.
    struct ArmedCharacter
    {
        CharacterModel body;
        LocomotionSet rifle{};
        LocomotionSet aiming{};
    };

    // The character models loaded for the pawns and one animated character per player.
    // unarmed:     the unarmed body.
    // locomotion:  its locomotion clips.
    // crouch:      its crouched locomotion, or nothing when the clips are missing (the squash
    //              stand-in is used then).
    // armed:       the armed body, or nothing (armed pawns then use the unarmed body).
    // firstPerson: the viewmodel, or nothing (armed players then see no weapon).
    // pawns:       Player1's and Player2's characters.
    // viewModel:   the local player's viewmodel state.
    // pose:        the crouch and slide stand-in pose.
    struct PawnCharacters
    {
        CharacterModel unarmed;
        LocomotionSet locomotion{};
        std::optional<LocomotionSet> crouch{};
        std::optional<ArmedCharacter> armed{};
        std::optional<FirstPersonModel> firstPerson{};
        std::array<PawnCharacter, k_playerCount> pawns{};
        ViewModelState viewModel{};
        PawnPoseTuning pose{};
    };

    // Loads the k_pawnCharacter models from the mounted content and logs their stride speeds.
    // renderer: uploads the models.
    // files:    the mounted content.
    // Returns:  the characters, or nothing when the unarmed body isn't mounted or can't be used
    //           (the reason is logged; pawns are then drawn as boxes). A missing armed body or
    //           viewmodel is logged and left out.
    [[nodiscard]] std::optional<PawnCharacters> loadPawnCharacters(lockstep::render::Renderer& renderer,
        const lockstep::assets::FileSystem& files);

    // Starts a presentation frame: remembers which pawns were drawn in the previous one.
    // characters: the pawn characters.
    void beginPawnCharacters(PawnCharacters& characters);

    // Queues a player pawn for drawing: the animated character (armed or unarmed body, playing
    // the unarmed, rifle or aiming locomotion) standing at the capsule bottom and facing the
    // pawn's yaw, or the coloured box without characters.
    // renderer:     the renderer.
    // visuals:      the boxes and colours of the fallback.
    // characters:   the pawn characters, or null to draw boxes.
    // owner:        the pawn's player (Player1 or Player2); picks the character and the colour.
    // transform:    the pawn's interpolated or predicted transform.
    // look:         armed, aiming, lowered, crouch and slide.
    // deltaSeconds: Player time since the previous frame; animates the character.
    // layers:       render layers to draw on (k_mirrorOnlyLayer for the local pawn).
    void drawPawn(lockstep::render::Renderer& renderer, const Visuals& visuals, PawnCharacters* characters,
        lockstep::world::EntityOwner owner, const math::Transform& transform, const PawnLook& look,
        float deltaSeconds, lockstep::render::RenderLayers layers = lockstep::render::k_defaultRenderLayer);

    // owner:   a pawn's player.
    // Returns: the planar speed its character was last drawn moving at, m/s (0 without one).
    [[nodiscard]] float pawnCharacterSpeed(const PawnCharacters& characters, lockstep::world::EntityOwner owner);

#if LOCKSTEP_DEV_TOOLS
    // Draws the pawn characters section of the Game dev panel: the models, stride speeds, what
    // each pawn is playing and the viewmodel tuning.
    // characters: the pawn characters, or null when pawns are boxes.
    void pawnCharactersDevSection(PawnCharacters* characters);
#endif
} //namespace game

#endif //LOCKSTEP_GAME_CHARACTERS_PAWN_CHARACTERS_HPP
