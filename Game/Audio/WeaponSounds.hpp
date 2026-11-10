#ifndef LOCKSTEP_GAME_AUDIO_WEAPON_SOUNDS_HPP
#define LOCKSTEP_GAME_AUDIO_WEAPON_SOUNDS_HPP

#include "Audio/Surfaces.hpp"

#include <Lockstep/Audio/AudioTypes.hpp>
#include <Lockstep/Core/Math.hpp>

#include <array>
#include <cstdint>
#include <optional>
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
    // The weapon detail sounds, loaded from `sounds/` (built by
    // Content/source/scripts/make_weapon_fx_sounds.py). A missing file leaves that sound silent.
    // magazineOut, magazineIn, charge: the reload's parts (`weapons/v45_mag_out`, `v45_mag_in`,
    //                  `v45_charge`).
    // casings:         per surface, brass landing (`casings/<surface>_<n>`).
    // magazines:       per surface, a dropped magazine landing (`magazines/<surface>_<n>`).
    // indoorTails:     the room's short reflections after a gunshot (`tails/indoor_<n>`).
    // outdoorTails:    the long echo outdoors (`tails/outdoor_<n>`).
    struct WeaponSoundBank
    {
        std::optional<lockstep::audio::SoundHandle> magazineOut;
        std::optional<lockstep::audio::SoundHandle> magazineIn;
        std::optional<lockstep::audio::SoundHandle> charge;
        std::array<std::vector<lockstep::audio::SoundHandle>, k_surfaceCount> casings{};
        std::array<std::vector<lockstep::audio::SoundHandle>, k_surfaceCount> magazines{};
        std::vector<lockstep::audio::SoundHandle> indoorTails;
        std::vector<lockstep::audio::SoundHandle> outdoorTails;
    };

    // Volumes, editable from the dev panel.
    // reload:      the reload parts.
    // casing:      a casing landing at full speed.
    // magazine:    a magazine landing at full speed.
    // indoorTail:  the room reflections at full indoor weight.
    // outdoorTail: the outdoor echo at full outdoor weight.
    // maxTails:    tails started per frame at most (automatic fire of many shooters).
    struct WeaponSoundTuning
    {
        float reload{0.75f};
        float casing{0.35f};
        float magazine{0.6f};
        float indoorTail{0.45f};
        float outdoorTail{0.6f};
        int maxTails{3};
    };

    // The weapon sounds and their random state. Presentation only.
    struct WeaponSounds
    {
        WeaponSoundBank bank{};
        WeaponSoundTuning tuning{};
        uint32_t random{0x5BD1E995u};
        int tailsThisFrame{0};
    };

    // Loads the weapon sounds found under `sounds/`, logging what is missing.
    // audio:   the audio system.
    // files:   the mounted content.
    // Returns: the loaded sounds.
    [[nodiscard]] WeaponSoundBank loadWeaponSounds(lockstep::audio::AudioSystem& audio,
        const lockstep::assets::FileSystem& files);

    // Plays one sound of a list at a point (random pick, slight pitch and volume variation).
    // sounds:   the weapon sounds.
    // audio:    the audio system.
    // list:     the variations; empty plays nothing.
    // position: where.
    // volume:   linear gain.
    // bus:      the bus.
    void playWeaponSound(WeaponSounds& sounds, lockstep::audio::AudioSystem& audio,
        const std::vector<lockstep::audio::SoundHandle>& list, const math::Vec3& position, float volume,
        lockstep::audio::Bus bus = lockstep::audio::Bus::World);

    // Plays the reverb tail of a gunshot: the indoor reflections and the outdoor echo, crossfaded
    // by the listener's indoor weight. At most tuning.maxTails per frame (see beginWeaponSounds()).
    // sounds:  the weapon sounds.
    // audio:   the audio system.
    // shot:    where the shot was fired.
    // indoor:  the listener's indoor weight, 0 outdoors to 1 indoors (CombatAudio::indoorWeight).
    void playGunshotTail(WeaponSounds& sounds, lockstep::audio::AudioSystem& audio, const math::Vec3& shot, float indoor);

    // Starts a frame: resets the per-frame tail count.
    // sounds: the weapon sounds.
    void beginWeaponSounds(WeaponSounds& sounds);
} //namespace game

#endif //LOCKSTEP_GAME_AUDIO_WEAPON_SOUNDS_HPP
