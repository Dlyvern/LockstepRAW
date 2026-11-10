#ifndef LOCKSTEP_GAME_AUDIO_FOOTSTEPS_HPP
#define LOCKSTEP_GAME_AUDIO_FOOTSTEPS_HPP

#include "Audio/Surfaces.hpp"
#include "Player/Pawns.hpp"

#include <Lockstep/Audio/AudioTypes.hpp>
#include <Lockstep/Core/Math.hpp>
#include <Lockstep/Physics/PhysicsTypes.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace lockstep::assets
{
    class FileSystem;
} //namespace lockstep::assets

namespace lockstep::audio
{
    class AudioSystem;
} //namespace lockstep::audio

namespace game
{
    // Most variations of one footstep sound loaded per surface.
    inline constexpr uint32_t k_maxFootstepVariations{8};

    // How loud footsteps and landings are, editable from the dev panel.
    // walkVolume, runVolume: step volume at walk and at run speed (blended between).
    // crouchVolume:  step volume while crouched.
    // landMinSpeed:  falls slower than this (m/s) land silently.
    // landFullSpeed: fall speed (m/s) of the loudest landing.
    // landVolume:    volume of the loudest landing.
    // pitchJitter:   random pitch change per step, as a fraction.
    // volumeJitter:  random volume change per step, as a fraction.
    // bulletTimePitch, bulletTimeVolume: multipliers while the World domain is slowed. Footsteps
    //                play on the Player bus, which bullet-time leaves alone; this only makes them
    //                feel heavier.
    // remoteFallSpeed: downward speed (m/s) from which another player's pawn counts as falling.
    // vaultVolume:   volume of the vault sound (hand plant and scuff).
    // kickSwingVolume: volume of the kick's whoosh.
    // kickHitVolume: volume of the thud of a kick that struck something.
    struct FootstepTuning
    {
        float walkVolume{0.35f};
        float runVolume{0.8f};
        float crouchVolume{0.15f};
        float landMinSpeed{3.0f};
        float landFullSpeed{10.0f};
        float landVolume{1.0f};
        float pitchJitter{0.06f};
        float volumeJitter{0.12f};
        float bulletTimePitch{0.88f};
        float bulletTimeVolume{0.8f};
        float remoteFallSpeed{2.5f};
        float vaultVolume{0.75f};
        float kickSwingVolume{0.6f};
        float kickHitVolume{1.0f};
    };

    // The loaded footstep sounds, per surface. A surface without its own sounds uses Concrete's.
    // steps:    step variations (`sounds/footsteps/<surface>_<n>.wav` or `.ogg`, n from 1).
    // landings: landing variations (`sounds/footsteps/<surface>_land_<n>.wav`); steps are used
    //           where there are none.
    // vaults:   vault variations (`sounds/player/vault_<n>.wav`), played as a vault starts.
    // kickSwings: kick whooshes (`sounds/player/kick_swing_<n>.wav`), played as a kick starts.
    // kickHits: kick thuds (`sounds/player/kick_hit_<n>.wav`).
    struct FootstepBank
    {
        std::array<std::vector<lockstep::audio::SoundHandle>, k_surfaceCount> steps{};
        std::array<std::vector<lockstep::audio::SoundHandle>, k_surfaceCount> landings{};
        std::vector<lockstep::audio::SoundHandle> vaults{};
        std::vector<lockstep::audio::SoundHandle> kickSwings{};
        std::vector<lockstep::audio::SoundHandle> kickHits{};
    };

    // Footfall tracking of one pawn drawn this frame. Presentation only.
    // lastFoot:     foot position last frame (other players' pawns).
    // hasLast:      lastFoot is valid.
    // distance:     planar metres walked since the last step (other players' pawns).
    // fallDistance: metres fallen in the current fall (other players' pawns).
    // fallSpeed:    fastest fall seen in it.
    // footfalls, landings: the local head motion's counters last seen (local pawn).
    // surface:      the surface of the newest step.
    // wasVaulting:  the pawn vaulted last frame.
    // wasKicking:   the pawn kicked last frame.
    struct FootstepTracker
    {
        math::Vec3 lastFoot{0.0f, 0.0f, 0.0f};
        bool hasLast{false};
        float distance{0.0f};
        float fallDistance{0.0f};
        float fallSpeed{0.0f};
        uint32_t footfalls{0};
        uint32_t landings{0};
        Surface surface{Surface::Concrete};
        bool wasVaulting{false};
        bool wasKicking{false};
    };

    // Footstep sounds of both players. Presentation only, never replicated.
    // bank:      the loaded sounds.
    // tuning:    volumes and pitch.
    // pawns:     per player, footfall tracking.
    // random:    state of the presentation-only random generator (variation, jitter).
    // lastPicks: per surface, the variation played last, so it does not repeat at once.
    // played:    sounds started so far, for the dev panel.
    struct Footsteps
    {
        FootstepBank bank{};
        FootstepTuning tuning{};
        std::array<FootstepTracker, k_playerCount> pawns{};
        uint32_t random{0x9E3779B9u};
        std::array<uint32_t, k_surfaceCount> lastPicks{};
        uint32_t played{0};
    };

    // One pawn drawn this frame, as footsteps need it.
    // index:      the player's pawn index.
    // foot:       its drawn foot position.
    // controller: its controller on this machine (ignored by the surface ray), or null.
    // isLocal:    this machine's player: steps follow the head motion's footfalls (the camera bob)
    //             and landings; otherwise they follow the distance it is drawn moving.
    // footfalls:  local only: HeadPose::footfalls.
    // landings:   local only: HeadPose::landings.
    // landSpeed:  local only: HeadPose::landSpeed.
    // speed:      local only: its planar speed, m/s (others' is measured).
    // isCrouched: crouched or sliding (quieter, shorter strides).
    // isVaulting: vaulting over an obstacle (the vault sound plays as it starts).
    // isKicking:  kicking (the whoosh plays as it starts).
    struct FootstepPawn
    {
        std::size_t index{0};
        math::Vec3 foot{0.0f, 0.0f, 0.0f};
        lockstep::physics::ControllerId controller{};
        bool isLocal{false};
        uint32_t footfalls{0};
        uint32_t landings{0};
        float landSpeed{0.0f};
        float speed{0.0f};
        bool isCrouched{false};
        bool isVaulting{false};
        bool isKicking{false};
    };

    // What the footsteps of one frame depend on besides the pawns.
    // level:       the loaded level (surface lookup), or null.
    // physics:     the physics world (surface ray).
    // walkSpeed, runSpeed: movement speeds the volume blends between.
    // walkStride, runStride, crouchStride: metres per stride cycle (two steps) at those gaits,
    //              the head motion's values, so other players step like the local camera bobs.
    // walkStrideSpeed, runStrideSpeed: speeds at which the walk and run strides apply (the head
    //              motion's values).
    // worldScale:  the World time domain's scale (below 1 in bullet-time).
    // seconds:     Player time since the previous frame.
    struct FootstepFrame
    {
        const SceneLevel* level{nullptr};
        const lockstep::physics::PhysicsWorld* physics{nullptr};
        float walkSpeed{3.8f};
        float runSpeed{6.4f};
        float walkStride{1.5f};
        float runStride{2.4f};
        float crouchStride{1.1f};
        float walkStrideSpeed{1.6f};
        float runStrideSpeed{3.75f};
        float worldScale{1.0f};
        float seconds{0.0f};
    };

    // Loads every footstep and landing variation found under `sounds/footsteps/` as 3D sounds.
    // Logs what was found; when nothing is, logs once the file names it looks for.
    // footsteps: receives the sounds.
    // audio:     the audio system.
    // files:     the mounted content (Data/ is mounted by default).
    void loadFootsteps(Footsteps& footsteps, lockstep::audio::AudioSystem& audio,
        const lockstep::assets::FileSystem& files);

    // Plays the footsteps and landings of the pawns drawn this frame on the Player bus, at their
    // feet, picking the sound by the surface under them. Pawns not listed restart their tracking.
    // footsteps: the footstep state, updated.
    // audio:     the audio system.
    // frame:     level, speeds, strides, bullet-time and frame time.
    // pawns:     the pawns drawn this frame.
    void updateFootsteps(Footsteps& footsteps, lockstep::audio::AudioSystem& audio, const FootstepFrame& frame,
        std::span<const FootstepPawn> pawns);

    // Plays the thud of a kick that struck something.
    // footsteps:  the footstep state (its kick sounds).
    // audio:      the audio system.
    // position:   where it struck.
    // worldScale: the World time domain's scale (bullet-time makes it heavier).
    void playKickHit(Footsteps& footsteps, lockstep::audio::AudioSystem& audio, const math::Vec3& position,
        float worldScale);

#if LOCKSTEP_DEV_TOOLS
    // Draws the footstep tuning controls of the Game dev panel.
    // footsteps: the footstep state.
    void footstepsDevSection(Footsteps& footsteps);
#endif
} //namespace game

#endif //LOCKSTEP_GAME_AUDIO_FOOTSTEPS_HPP
