#ifndef LOCKSTEP_GAME_CHARACTERS_PAWN_CHARACTERS_HPP
#define LOCKSTEP_GAME_CHARACTERS_PAWN_CHARACTERS_HPP

#include "Characters/ArmedCrouch.hpp"
#include "Characters/CharacterModel.hpp"
#include "Characters/FirstPersonBody.hpp"
#include "Characters/HeldGuns.hpp"
#include "Characters/KickPose.hpp"
#include "Characters/Locomotion.hpp"
#include "Characters/NpcBodies.hpp"
#include "Characters/ViewModel.hpp"
#include "Npcs/HitReactions.hpp"
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
    // isVaulting: the pawn vaults over an obstacle (a procedural side vault: no clip exists).
    // isKicking: the pawn kicks (a procedural front kick, see KickPose.hpp; starts when it turns on).
    // weapon:   the loadout weapon in its hands (the M4 and Glock are drawn as held guns).
    struct PawnLook
    {
        bool isArmed{false};
        bool isAiming{false};
        bool isLowered{false};
        float crouch{0.0f};
        bool isSliding{false};
        bool isVaulting{false};
        bool isKicking{false};
        WeaponKind weapon{WeaponKind::M4};
    };

    // Stand-in for the crouch and slide clips that are missing (armed crouch, slide): the
    // character is squashed towards its feet and leans back while sliding. Editable from the dev
    // panel.
    // crouchSquash:  fraction of height taken off when fully crouched without crouch clips.
    // crouchClipThreshold: crouch amount from which the unarmed crouch clips play.
    // slideSquash:   extra fraction taken off while sliding.
    // slideLean:     backward lean while sliding, radians.
    // slideBlendRate: how fast the slide pose blends in and out, 1/s.
    // armedCrouchFade: how long the armed crouch's upper body layer takes to come in or go, s.
    // vaultRoll:     sideways tilt of the procedural vault (legs swing over to the side), radians.
    // vaultLean:     forward lean of the vault, radians.
    // vaultSquash:   fraction of height taken off while vaulting (legs tucked).
    // vaultPivot:    height above the feet the vault pose turns about (the hips), metres.
    // vaultBlendRate: how fast the vault pose blends in and out, 1/s.
    struct PawnPoseTuning
    {
        float crouchSquash{0.3f};
        float crouchClipThreshold{0.5f};
        float slideSquash{0.1f};
        float slideLean{0.35f};
        float slideBlendRate{10.0f};
        float armedCrouchFade{0.25f};
        float vaultRoll{0.55f};
        float vaultLean{0.3f};
        float vaultSquash{0.22f};
        float vaultPivot{0.9f};
        float vaultBlendRate{16.0f};
    };

    // One player pawn's animated character. Presentation only, never replicated.
    // instance:      pose and animator.
    // locomotion:    movement observed from where the pawn is drawn.
    // isDrawn:       drawn this frame.
    // wasDrawn:      drawn last frame; a pawn that skipped a frame (dead, absent) restarts tracking.
    // isArmedModel:  drawn with the armed body last time; a switch restarts tracking.
    // slideBlend:    0 upright to 1 in the slide pose, eased.
    // vaultBlend:    0 upright to 1 in the vault pose, eased.
    // kickSeconds:   presentation seconds into the kick shown, or negative without one.
    // wasKicking:    PawnLook::isKicking last frame.
    // placement:     the world placement it was drawn with this frame.
    // yaw:           the pawn's yaw this frame.
    // reaction:      procedural hit reaction on top of its animation (other players only).
    // reactionRig:   the joints it turns on the model last drawn.
    // weapon:        the loadout weapon it was last drawn holding.
    struct PawnCharacter
    {
        lockstep::anim::SkinnedInstance instance{};
        LocomotionState locomotion{};
        bool isDrawn{false};
        bool wasDrawn{false};
        bool isArmedModel{false};
        float slideBlend{0.0f};
        float vaultBlend{0.0f};
        float kickSeconds{-1.0f};
        bool wasKicking{false};
        math::Transform placement{};
        float yaw{0.0f};
        HitReaction reaction{};
        ReactionRig reactionRig{};
        WeaponKind weapon{WeaponKind::M4};
    };

    // The armed body, its two locomotion sets, its crouch (nothing when the model lacks the
    // clips) and the parts the first-person camera may show (nothing for a model cooked without
    // limbs).
    struct ArmedCharacter
    {
        CharacterModel body;
        LocomotionSet rifle{};
        LocomotionSet aiming{};
        std::optional<FirstPersonBodyMasks> firstPersonBody{};
        std::optional<ArmedCrouch> crouch{};
    };

    // The character models loaded for the pawns and one animated character per player.
    // unarmed:     the unarmed body.
    // locomotion:  its locomotion clips.
    // crouch:      its crouched locomotion, or nothing when the clips are missing (the squash
    //              stand-in is used then).
    // armed:       the armed body, or nothing (armed pawns then use the unarmed body).
    // firstPersons: per loadout weapon, its viewmodel, or nothing (that weapon then shows the
    //              KRISS', or none at all without it).
    // heldGuns:    per loadout weapon, the gun a third-person body holds instead of the armed
    //              model's KRISS (none for the KRISS itself, or when not mounted).
    // pawns:       Player1's and Player2's characters.
    // viewModel:   the local player's viewmodel state.
    // pose:        the crouch and slide stand-in pose.
    // unarmedFirstPersonBody: the unarmed body's parts the first-person camera may show, or
    //              nothing for a model cooked without limbs.
    // firstPersonBody: how the local body shows in the first-person camera.
    // kickPose:    how the procedural kick looks.
    // reactionTuning: how hit reactions on the pawns look.
    // npcBodies:   the other character models NPCs may be drawn with (see NpcBody).
    struct PawnCharacters
    {
        CharacterModel unarmed;
        LocomotionSet locomotion{};
        std::optional<LocomotionSet> crouch{};
        std::optional<ArmedCharacter> armed{};
        std::array<std::optional<FirstPersonModel>, k_weaponCount> firstPersons{};
        std::array<std::optional<HeldGun>, k_weaponCount> heldGuns{};
        std::array<PawnCharacter, k_playerCount> pawns{};
        ViewModelState viewModel{};
        PawnPoseTuning pose{};
        std::optional<FirstPersonBodyMasks> unarmedFirstPersonBody{};
        FirstPersonBodyTuning firstPersonBody{};
        HitReactionTuning reactionTuning{.impulse = 4.5f, .staggerImpulse = 5.0f};
        KickPoseTuning kickPose{};
        NpcBodies npcBodies{};
    };

    // The viewmodel of a loadout weapon: its own, else the KRISS' as a stand-in.
    // characters: the pawn characters.
    // weapon:     the weapon.
    // Returns:    the model, or null when none is loaded.
    [[nodiscard]] const FirstPersonModel* firstPersonModel(const PawnCharacters& characters, WeaponKind weapon);

    // The held gun of a loadout weapon.
    // characters: the pawn characters.
    // weapon:     the weapon.
    // Returns:    the gun, or null for the KRISS (part of the armed body) or when not loaded.
    [[nodiscard]] const HeldGun* heldGun(const PawnCharacters& characters, WeaponKind weapon);

    // Loads the k_pawnCharacter models and the NPC bodies from the mounted content and logs their
    // stride speeds.
    // renderer: uploads the models.
    // files:    the mounted content.
    // Returns:  the characters, or nothing when the unarmed body isn't mounted or can't be used
    //           (the reason is logged; pawns are then drawn as boxes). A missing armed body or
    //           viewmodel is logged and left out.
    [[nodiscard]] std::optional<PawnCharacters> loadPawnCharacters(lockstep::render::Renderer& renderer,
        const lockstep::assets::FileSystem& files);

    // Ground distance of one stride cycle (two steps) of the unarmed character's clips, metres:
    // each clip's stride speed times its length (playback is scaled to the ground speed, so the
    // cycle length stays the same). 0 where a clip is missing.
    // walk, run, crouch: the walk, run and crouched walk clips.
    // walkSpeed, runSpeed: the walk and run clips' stride speeds, m/s (0 where missing).
    struct StrideLengths
    {
        float walk{0.0f};
        float run{0.0f};
        float crouch{0.0f};
        float walkSpeed{0.0f};
        float runSpeed{0.0f};
    };

    // characters: the pawn characters.
    // Returns:    the stride cycle lengths of their clips.
    [[nodiscard]] StrideLengths pawnStrideLengths(const PawnCharacters& characters);

    // Starts a presentation frame: remembers which pawns were drawn in the previous one.
    // characters: the pawn characters.
    void beginPawnCharacters(PawnCharacters& characters);

    // Queues a player pawn for drawing: the animated character (armed or unarmed body, playing
    // the unarmed, rifle or aiming locomotion) standing at the capsule bottom and facing the
    // pawn's yaw, or the coloured box without characters.
    // renderer:     the renderer.
    // visuals:      the boxes and colours of the fallback.
    // characters:   the pawn characters, or null to draw boxes.
    // owner:        the pawn's player (Player1 or Player2); picks the character, its head gear
    //               (playerGearLook()) and the colour.
    // transform:    the pawn's interpolated or predicted transform.
    // look:         armed, aiming, lowered, crouch and slide.
    // deltaSeconds: Player time since the previous frame; animates the character.
    // layers:       render layers to draw on (k_mirrorOnlyLayer for the local pawn).
    // firstPerson:  for the local pawn, the first-person look: the body is drawn a second time on
    //               k_firstPersonBodyLayer, behind the eye and without head and head gear
    //               (full-body awareness); null for other pawns.
    void drawPawn(lockstep::render::Renderer& renderer, const Visuals& visuals, PawnCharacters* characters,
        lockstep::world::EntityOwner owner, const math::Transform& transform, const PawnLook& look,
        float deltaSeconds, lockstep::render::RenderLayers layers = lockstep::render::k_defaultRenderLayer,
        const FirstPersonBodyView* firstPerson = nullptr);

    // Makes a pawn's character flinch from a hit (presentation only; the local player's own body
    // never does, so the first-person view stays steady).
    // characters: the pawn characters.
    // owner:      the pawn's player.
    // hit:        the hit.
    void reactToPawnHit(PawnCharacters& characters, lockstep::world::EntityOwner owner, const ReactionHit& hit);

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
