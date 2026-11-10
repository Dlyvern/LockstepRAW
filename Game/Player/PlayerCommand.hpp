#ifndef LOCKSTEP_GAME_PLAYER_PLAYER_COMMAND_HPP
#define LOCKSTEP_GAME_PLAYER_PLAYER_COMMAND_HPP

#include "Player/KeyBindings.hpp"
#include "Weapons/FireModes.hpp"
#include "Weapons/OpticKind.hpp"
#include "Weapons/Recoil.hpp"
#include "Weapons/WeaponKind.hpp"
#include "Weapons/WeaponStance.hpp"

#include <Lockstep/Core/Math.hpp>
#include <Lockstep/Platform/Input.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

namespace game
{
    namespace math = lockstep::math;

    struct WeaponStats;

    // Per-weapon stats of the loadout (Weapons/WeaponCatalog.hpp).
    using WeaponStatsTable = std::array<WeaponStats, k_weaponCount>;

    // Bytes an encoded PlayerCommand takes.
    inline constexpr std::size_t k_playerCommandBytes{19};

    // What one player wants to do this tick. The client sends it to the host every tick; the host
    // builds its own player's from local input. The host simulates both players from these.
    // move:       wanted movement in the pawn's frame, x to the right and y forward, each in [-1, 1].
    // yaw:        look direction around the up axis, in radians. 0 looks along -z.
    // pitch:      look direction up (positive) or down, in radians.
    // shotCount:  shots fired so far, wrapping at 256. The host fires once per increase, so a lost
    //             or repeated message neither drops nor doubles a shot (up to 255 at a time).
    // bulletTime: true while the player holds bullet-time (Left Alt or the middle mouse button).
    // interactCount: interact presses (F) so far, wrapping at 256; counted like `shotCount`.
    // sprint:     true while the player holds run (Shift); only speeds up forward moves.
    // aim:        true while the armed player aims down sights (right mouse button, not while the
    //             weapon must stay lowered); no running then.
    // lower:      the player's lower toggle (X): wants the weapon lowered where that is allowed.
    // isWeaponUp: the player is armed with the weapon raised; caps running at
    //             PawnSettings::weaponUpRunSpeed. The client sets it to predict its own pawn; the
    //             host overwrites it from its own armed and lowered state.
    // crouch:     the player wants to crouch (Ctrl held or the C toggle on). A fresh crouch while
    //             running starts a slide (see stepPawn()).
    // jump:       the player holds jump (Space). A fresh press jumps (or mantles, see stepPawn()).
    // lean:       -1 while leaning left (Q), 1 right (E), 0 upright.
    // reloadCount: reload presses (R) so far, wrapping at 256; counted like `shotCount`.
    // isInteractHeld: the player holds interact (F), e.g. to revive a downed partner.
    // aimOffset:  (yaw, pitch) radians from the look to where the shot goes: the sight line of the
    //             drawn weapon (sway, weapon kick) and the camera's head motion, as the shooter
    //             saw them. The host clamps it (SpreadSettings::maxAimOffset); see shotDirection().
    // isFlashlightOn: the weapon flashlight is on (replicated to the partner via PlayerStatus).
    // kick:       the player holds kick (V). A fresh press kicks (see PawnKick.hpp).
    // optic:      the optic on the player's weapon in hand (the loadout choice; replicated to the
    //             partner via PlayerStatus, presentation only).
    // weapon:     the loadout weapon the player wants in hand (1, 2, 3 or the mouse wheel); the
    //             host switches to it (PlayerStatus::weapon).
    struct PlayerCommand
    {
        math::Vec2 move{0.0f, 0.0f};
        float yaw{0.0f};
        float pitch{0.0f};
        uint8_t shotCount{0};
        bool bulletTime{false};
        uint8_t interactCount{0};
        bool sprint{false};
        bool aim{false};
        bool lower{false};
        bool isWeaponUp{false};
        bool crouch{false};
        bool jump{false};
        int8_t lean{0};
        uint8_t reloadCount{0};
        bool isInteractHeld{false};
        math::Vec2 aimOffset{0.0f, 0.0f};
        bool isFlashlightOn{false};
        bool kick{false};
        OpticKind optic{OpticKind::None};
        WeaponKind weapon{WeaponKind::M4};
    };

    // The local player's look, shot and interact counters, kept between ticks to build
    // PlayerCommands.
    // yaw, pitch:    accumulated mouse look, in radians.
    // shotCount:     shots fired so far, wrapping at 256.
    // interactCount: interact presses so far, wrapping at 256.
    // wantsLowered:  the lower toggle; fire or aim clears it to raise the weapon.
    // isCrouchToggled: the crouch toggle (C); a fresh press of run (Shift) or jump (Space) clears it.
    // reloadCount:   reload presses so far, wrapping at 256.
    // trigger:       fire mode, cadence and predicted rounds.
    // recoil:        the camera recoil, added to the look in each command.
    // isFlashlightOn: the flashlight toggle.
    // opticSensitivityScale: multiplier on the aimed look sensitivity from the optic aimed
    //                through (1 / magnification^power), set by the presentation each frame.
    // weapon:        the weapon asked for (1, 2, 3, mouse wheel).
    // previousWeapon: the weapon asked for before it (put away during the switch).
    // switchSeconds: Player seconds left of the predicted switch to it (no shots meanwhile).
    struct LocalControls
    {
        float yaw{0.0f};
        float pitch{0.0f};
        uint8_t shotCount{0};
        uint8_t interactCount{0};
        bool wantsLowered{false};
        bool isCrouchToggled{false};
        uint8_t reloadCount{0};
        Trigger trigger{};
        RecoilState recoil{};
        bool isFlashlightOn{false};
        float opticSensitivityScale{1.0f};
        WeaponKind weapon{WeaponKind::M4};
        WeaponKind previousWeapon{WeaponKind::M4};
        float switchSeconds{0.0f};
    };

    // The fire and aim buttons of one tick.
    // isPressed: fire was pressed this tick.
    // isHeld:    fire is held.
    // isAiming:  aim is held.
    struct FireButtons
    {
        bool isPressed{false};
        bool isHeld{false};
        bool isAiming{false};
    };

    // What decides how fire, aim and the lower key act this tick, as the local machine predicts it.
    // isArmed:      the player carries a weapon (as last replicated).
    // zone:         the weapon zone the local pawn stands in.
    // isHoldingToy: the player carries a toy: the weapon is lowered (as in a safe zone), a click
    //               throws the toy, aim and the lower key do nothing.
    // isReady:      the weapon may fire (not running or just stopped, isWeaponReady() of the
    //               local pawn as last simulated or predicted); a click while not ready is ignored,
    //               as the host would ignore the shot.
    // magazine:     rounds in the magazine (as last replicated); the trigger predicts its own
    //               shots on top, and a click on an empty magazine asks for a reload instead.
    // isReloading:  a reload runs (as last replicated).
    // weapons:      every weapon's fire rate, modes, recoil and switch times (host settings); the
    //               one in LocalControls::weapon is in use. Must not be null.
    // aimOffset:    the sight offset of the newest presented frame (see PlayerCommand::aimOffset).
    // canToggleFlashlight: F toggles the flashlight this tick (nothing to interact with or revive).
    // scriptedFire: dev drills only: replaces the fire and aim buttons (no captured cursor needed).
    struct WeaponControl
    {
        bool isArmed{false};
        WeaponZone zone{WeaponZone::Open};
        bool isHoldingToy{false};
        bool isReady{true};
        int32_t magazine{0};
        bool isReloading{false};
        const WeaponStatsTable* weapons{nullptr};
        math::Vec2 aimOffset{0.0f, 0.0f};
        bool canToggleFlashlight{false};
        std::optional<FireButtons> scriptedFire{};
    };

    // Settings for turning device input into commands. Mouse movement is the raw relative motion
    // (the window enables raw mouse motion while the cursor is captured), applied linearly: no
    // smoothing, no acceleration.
    // mouseSensitivity: radians of look per pixel of mouse movement.
    // aimSensitivityScale: sensitivity multiplier while aiming down sights.
    // maxPitch:         largest look angle up or down, in radians.
    // invertY:          moving the mouse up looks down.
    // bindings:         the keys and buttons of every action (options menu, settings file).
    struct ControlSettings
    {
        float mouseSensitivity{0.0025f};
        float aimSensitivityScale{0.7f};
        float maxPitch{1.45f};
        bool invertY{false};
        KeyBindings bindings{defaultKeyBindings()};
    };

    // settings:   invert-Y setting.
    // mouseDelta: raw mouse movement in pixels.
    // Returns:    the movement that turns the look (y flipped when settings.invertY).
    [[nodiscard]] math::Vec2 lookDelta(const ControlSettings& settings, math::Vec2 mouseDelta);

    // settings:    sensitivity settings.
    // isAiming:    the player aims down sights.
    // opticScale:  multiplier while aiming through a magnified optic (LocalControls::opticSensitivityScale).
    // Returns:     radians of look per pixel of mouse movement.
    [[nodiscard]] float lookSensitivity(const ControlSettings& settings, bool isAiming, float opticScale = 1.0f);

    // Builds this tick's command from local input through ControlSettings::bindings (the defaults
    // are named here): WASD to move, Shift to run, hold Ctrl or toggle
    // C to crouch (Ctrl or C while running slides), Space to jump (a press also clears the crouch
    // toggle), hold Q / E to lean, mouse to look, left mouse to fire (by the fire mode, B cycles
    // it within the weapon's modes), 1 / 2 / 3 or the mouse wheel to pick the M4, the Glock or
    // the KRISS (no shots while switching), right mouse to aim down sights, R to reload, V to kick, X (k_lowerWeaponKey) to lower
    // or raise the weapon, F to interact (held too, for reviving) or, with nothing to interact with, to toggle
    // the weapon flashlight, hold Left Alt or the middle mouse button for bullet-time. Look, fire,
    // aim, lower and interact only count while the cursor is captured. With the weapon lowered, a
    // click or aim raises it instead of firing (where raising is allowed; a click still throws a
    // held toy). Each shot kicks the camera recoil, which the command's look includes.
    // controls:        the local look and shot counter, updated in place.
    // settings:        sensitivity, invert-Y, pitch limit and key bindings.
    // input:           this tick's input.
    // isCursorCaptured: true while the window holds the mouse for mouse look.
    // weapon:          armed state, zone, held toy, rounds, fire rate, recoil and sight offset.
    // deltaSeconds:    the Player domain's delta of this tick.
    // Returns:         the command.
    [[nodiscard]] PlayerCommand sampleLocalCommand(LocalControls& controls, const ControlSettings& settings,
        const lockstep::platform::Input& input, bool isCursorCaptured, const WeaponControl& weapon, float deltaSeconds);

    // Limits a downed player's command to a crawl: crouched, slower, no running, jumping, leaning,
    // aiming or bullet-time. Host and client apply it alike, so the client's prediction matches;
    // applying it twice changes nothing.
    // command:    the command, changed in place.
    // crawlScale: longest move input allowed, 0 to 1 (DownedSettings::crawlScale).
    void restrictDownedCommand(PlayerCommand& command, float crawlScale);

    // Packs a command into the bytes sent to the host.
    // command: the command.
    // Returns: the encoded bytes.
    [[nodiscard]] std::array<std::byte, k_playerCommandBytes> encodeCommand(const PlayerCommand& command);

    // Unpacks a command the client sent.
    // bytes:   the received bytes.
    // Returns: the command, or nothing if `bytes` is not a valid command.
    [[nodiscard]] std::optional<PlayerCommand> decodeCommand(std::span<const std::byte> bytes);
} //namespace game

#endif //LOCKSTEP_GAME_PLAYER_PLAYER_COMMAND_HPP
