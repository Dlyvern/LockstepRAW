#ifndef LOCKSTEP_GAME_WEAPONS_WEAPON_FX_HPP
#define LOCKSTEP_GAME_WEAPONS_WEAPON_FX_HPP

#include "Audio/WeaponSounds.hpp"
#include "Effects/MuzzleFlashes.hpp"
#include "Effects/ViewModelFlash.hpp"
#include "Player/Pawns.hpp"
#include "Weapons/ReloadCues.hpp"
#include "Weapons/WeaponDebris.hpp"

#include <Lockstep/Core/Math.hpp>

#include <array>
#include <cstdint>
#include <unordered_map>

namespace lockstep::assets
{
    struct Scene;
} //namespace lockstep::assets

namespace lockstep::app
{
    struct EngineContext;
    struct GameState;
} //namespace lockstep::app

namespace game
{
    // Switches of the weapon details, editable from the dev panel.
    // casings:   spent casings are ejected.
    // magazines: reloads drop the empty magazine.
    // tails:     gunshots get the indoor reflections or outdoor echo.
    // ejectSpeed: sideways ejection speed range, m/s.
    // ejectLift: upward ejection speed range, m/s.
    struct WeaponFxTuning
    {
        bool casings{true};
        bool magazines{true};
        bool tails{true};
        math::Vec2 ejectSpeed{1.6f, 2.6f};
        math::Vec2 ejectLift{0.9f, 1.7f};
    };

    // The shot counters last seen per shooter, so each new shot is shown once.
    // hasLast: the counters are valid (false until the first frame).
    // local:   the local command's counter.
    // players: per player, its status counter.
    // npcs:    per NPC look entity index, its counter.
    struct ShotCounters
    {
        bool hasLast{false};
        uint8_t local{0};
        std::array<uint8_t, k_playerCount> players{};
        std::unordered_map<uint32_t, uint8_t> npcs{};
    };

    // Weapon details around the shots and reloads: muzzle flashes, lights, smoke and tracers,
    // ejected casings, dropped magazines, reload sounds timed to the reload clips, and gunshot
    // reverb tails. Presentation only, never replicated: each machine derives them from the shot
    // counters and statuses it already has, and simulates the debris itself (World time).
    // muzzle:    flashes, lights, barrel heat, tracers.
    // viewFlash: the local weapon's flash, drawn with the viewmodel.
    // casings, magazines: the debris.
    // reloads:   both players' reload timelines.
    // sounds:    the weapon sounds.
    // counters:  shot counters last seen.
    // tuning:    switches.
    // worldFov:  the world camera's vertical field of view this frame, radians (the first-person
    //            weapon's points are placed with it).
    // scene:     the level the debris lies in (cleared when it changes).
    // isLookTried: creating the debris meshes was attempted.
    // random:    presentation-only random state.
    struct WeaponFx
    {
        MuzzleFlashes muzzle{};
        ViewModelFlash viewFlash{};
        DebrisPool casings{};
        DebrisPool magazines{.kind = DebrisKind{.maxPieces = 12,
            .lifeSeconds = 120.0f,
            .halfThickness = 0.015f,
            .restitution = 0.2f,
            .friction = 0.5f,
            .spinKept = 0.4f,
            .spinKick = 3.0f,
            .restSpeed = 0.5f,
            .soundSpeed = 0.6f,
            .fullSoundSpeed = 4.0f,
            .maxSoundBounces = 2}};
        ReloadCues reloads{};
        WeaponSounds sounds{};
        ShotCounters counters{};
        WeaponFxTuning tuning{};
        float worldFov{1.047f};
        const lockstep::assets::Scene* scene{nullptr};
        bool isLookTried{false};
        uint32_t random{0x68E31DA4u};
    };

    // What the weapon details read each frame.
    // eye, aim:      the local camera position and look direction.
    // hasLocalPawn:  the local player has a pawn this frame.
    // worldFov:      the world camera's vertical field of view this frame, radians.
    // playerSeconds: Player time since the previous frame (reload timing).
    // worldSeconds:  World time since the previous frame (debris, flashes).
    struct WeaponFxFrame
    {
        math::Vec3 eye{0.0f, 0.0f, 0.0f};
        math::Vec3 aim{0.0f, 0.0f, -1.0f};
        bool hasLocalPawn{false};
        float worldFov{1.047f};
        float playerSeconds{0.0f};
        float worldSeconds{0.0f};
    };

    // Loads the weapon sounds (call once at start).
    // fx:     the weapon details.
    // engine: the engine (audio, files).
    void loadWeaponFx(WeaponFx& fx, lockstep::app::EngineContext& engine);

    // Shows this frame's shots and reload cues and moves the debris. Call after the pawns, the
    // NPCs and the viewmodel are drawn and the combat audio ran (indoor weight), and before
    // presentEffects() (which simulates and submits the particles).
    // state:  the game state (weapon details, statuses, characters, NPC looks, effects).
    // engine: the engine.
    // frame:  the camera and the frame's deltas.
    void presentWeaponFx(lockstep::app::GameState& state, lockstep::app::EngineContext& engine,
        const WeaponFxFrame& frame);

#if LOCKSTEP_DEV_TOOLS
    // Draws the weapon details section of the Game dev panel.
    // fx: the weapon details.
    void weaponFxDevSection(WeaponFx& fx);
#endif
} //namespace game

#endif //LOCKSTEP_GAME_WEAPONS_WEAPON_FX_HPP
