#ifndef LOCKSTEP_GAME_WEAPONS_WEAPON_FEEDBACK_HPP
#define LOCKSTEP_GAME_WEAPONS_WEAPON_FEEDBACK_HPP

#include "Player/PlayerStatus.hpp"
#include "Weapons/FireModes.hpp"

#include <Lockstep/Audio/AudioTypes.hpp>
#include <Lockstep/Core/Math.hpp>

#include <array>
#include <cstdint>
#include <optional>

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
    namespace math = lockstep::math;

    // The weapon handling sounds, loaded from `sounds/` (a missing file stays silent).
    // flashlightClick: `weapons/flashlight_click`, both players' lights.
    // fireModeSwitch:  `weapons/firemode_switch`, the local fire mode selector.
    // breathHold, breathRelease: `player/breath_hold`, `player/breath_release`, the local held
    //                  breath.
    struct HandlingSounds
    {
        std::optional<lockstep::audio::SoundHandle> flashlightClick;
        std::optional<lockstep::audio::SoundHandle> fireModeSwitch;
        std::optional<lockstep::audio::SoundHandle> breathHold;
        std::optional<lockstep::audio::SoundHandle> breathRelease;
    };

    // Volumes and the held-breath vision cue, editable from the dev panel.
    // clickVolume:  flashlight clicks.
    // switchVolume: fire mode switch.
    // breathVolume: held breath and exhale.
    // holdFocus:    depth of field strength while the breath holds the weapon steady (0 off).
    struct WeaponFeedbackTuning
    {
        float clickVolume{0.55f};
        float switchVolume{0.5f};
        float breathVolume{0.4f};
        float holdFocus{0.3f};
    };

    // Plays weapon handling sounds from the states this machine already has. Presentation only.
    // sounds, tuning: the sounds and their levels.
    // lastFlashlight: per player, the flashlight state last seen.
    // lastMode:     the local fire mode last seen.
    // lastHolds, lastReleases: the breath counters last seen.
    // hasLast:      the values above are valid.
    struct WeaponFeedback
    {
        HandlingSounds sounds{};
        WeaponFeedbackTuning tuning{};
        std::array<bool, k_playerCount> lastFlashlight{};
        FireMode lastMode{FireMode::Auto};
        uint32_t lastHolds{0};
        uint32_t lastReleases{0};
        bool hasLast{false};
    };

    // What weapon feedback reads this frame.
    // flashlightOn: per player, its light is on.
    // positions:    per player, where its light is (3D clicks), if drawn.
    // localIndex:   this machine's player.
    // mode:         the local fire mode.
    // holds, releases: the local breath counters (BreathState).
    struct WeaponFeedbackFrame
    {
        std::array<bool, k_playerCount> flashlightOn{};
        std::array<std::optional<math::Vec3>, k_playerCount> positions{};
        std::size_t localIndex{0};
        FireMode mode{FireMode::Auto};
        uint32_t holds{0};
        uint32_t releases{0};
    };

    // Loads the weapon handling sounds, logging what is missing.
    // audio:   the audio system.
    // files:   the mounted content.
    // Returns: the sounds.
    [[nodiscard]] HandlingSounds loadHandlingSounds(lockstep::audio::AudioSystem& audio,
        const lockstep::assets::FileSystem& files);

    // Plays the clicks and breaths of what changed since the last frame.
    // feedback: the feedback state.
    // audio:    the audio system.
    // frame:    this frame's states.
    void presentWeaponFeedback(WeaponFeedback& feedback, lockstep::audio::AudioSystem& audio,
        const WeaponFeedbackFrame& frame);

#if LOCKSTEP_DEV_TOOLS
    // Draws the weapon feedback controls of the Game dev panel.
    // feedback: the feedback state.
    void weaponFeedbackDevSection(WeaponFeedback& feedback);
#endif
} //namespace game

#endif //LOCKSTEP_GAME_WEAPONS_WEAPON_FEEDBACK_HPP
