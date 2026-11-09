#ifndef LOCKSTEP_AUDIO_AUDIO_TYPES_HPP
#define LOCKSTEP_AUDIO_AUDIO_TYPES_HPP

#include "Lockstep/Core/Math.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>

namespace lockstep::audio
{
    // Typed, generation-checked reference to something the AudioSystem owns. Cheap to copy and
    // safe to keep in game state: once what it named is gone, the AudioSystem treats it as stale.
    // The Tag only keeps handles of different kinds from mixing.
    template<typename Tag>
    struct AudioHandle
    {
        uint32_t index{0};
        uint32_t generation{0};

        // Returns: true unless this is the default (null) handle. A non-null handle may be stale.
        [[nodiscard]] constexpr bool isValid() const noexcept
        {
            return generation != 0;
        }

        friend constexpr bool operator==(AudioHandle, AudioHandle) noexcept = default;
    };

    // A loaded sound (decoded sample data, or a stream).
    using SoundHandle = AudioHandle<struct SoundTag>;

    // One playing instance of a sound. Goes stale when the voice stops, finishes, or is stolen
    // because too many voices play at once.
    using VoiceHandle = AudioHandle<struct VoiceTag>;

    // Mixer buses. Every voice plays on one; all of them feed Master.
    // Master:   everything. Its volume is the overall volume.
    // Music:    music (2D).
    // Ambience: room tones and background loops.
    // World:    sounds of the World time domain (doors, enemies, physics). The run loop gives it
    //           the World time scale, so bullet-time slows and muffles it.
    // Player:   the players' own sounds (footsteps, weapons). Always time scale 1, like the
    //           Player time domain.
    // Ui:       interface sounds (2D).
    enum class Bus : uint8_t
    {
        Master = 0,
        Music,
        Ambience,
        World,
        Player,
        Ui,
        Count
    };

    inline constexpr std::size_t k_busCount{static_cast<std::size_t>(Bus::Count)};

    // How a sound is loaded.
    // is3d:        positioned in the world and attenuated by distance (false: 2D, for UI and
    //              music).
    // isLooping:   voices of it loop until stopped.
    // isStreamed:  decoded while playing instead of up front (long music and ambience). The
    //              encoded file stays in memory while the sound exists. Each streamed sound plays
    //              on one voice at a time.
    // minDistance: 3D only: distance in metres up to which the sound is at full volume.
    // maxDistance: 3D only: distance in metres where attenuation stops (inverse roll-off).
    struct SoundDescription
    {
        bool is3d{true};
        bool isLooping{false};
        bool isStreamed{false};
        float minDistance{1.0f};
        float maxDistance{50.0f};
    };

    // How one voice starts.
    // bus:      the bus it plays on.
    // volume:   linear gain, 1 = as recorded.
    // pitch:    playback-rate multiplier, 1 = as recorded. The bus' time scale multiplies it.
    // position: 3D sounds only: where it starts, in world space. Without one, a 3D voice starts
    //           on the listener. Move it with AudioSystem::setVoicePosition().
    // velocity: 3D sounds only: its velocity in m/s, for Doppler.
    struct PlayDescription
    {
        Bus bus{Bus::World};
        float volume{1.0f};
        float pitch{1.0f};
        std::optional<math::Vec3> position{};
        math::Vec3 velocity{0.0f, 0.0f, 0.0f};
    };

    // Where the listener (the local player's ears) is. Right-handed, like the Engine's world.
    // position: in world space.
    // forward:  the direction the listener faces; need not be normalised.
    // up:       the listener's up; need not be normalised or exactly perpendicular to forward.
    struct ListenerPose
    {
        math::Vec3 position{0.0f, 0.0f, 0.0f};
        math::Vec3 forward{0.0f, 0.0f, -1.0f};
        math::Vec3 up{0.0f, 1.0f, 0.0f};
    };

    // How a bus reacts to a time scale below 1 (bullet-time).
    // followsPitch:     playback rate is multiplied by the time scale (sounds slow down and
    //                   drop in pitch).
    // lowPass:          a low-pass filter closes as the time scale drops.
    // lowPassCutoffHz:  cutoff at time scale 0; at scale 1 the filter is open. In between the
    //                   cutoff moves logarithmically.
    // smoothingSeconds: time constant with which the effect follows a changed scale, so a jump
    //                   into bullet-time ramps instead of clicking.
    struct TimeScaleEffect
    {
        bool followsPitch{true};
        bool lowPass{true};
        float lowPassCutoffHz{700.0f};
        float smoothingSeconds{0.12f};
    };

    // Settings for AudioSystem::create().
    // maxVoices:       voices that may exist at once; past that the quietest are made virtual
    //                  (silent, tracked) and real mixing is limited by the backend.
    // dopplerScale:    strength of the Doppler effect, 0 turns it off.
    // timeScaleEffect: how the World bus reacts to bullet-time (other buses start without it).
    // silent:          open no output device (FMOD's no-sound output): everything runs, nothing
    //                  is heard (tests, servers).
    struct AudioDescription
    {
        uint32_t maxVoices{256};
        float dopplerScale{0.5f};
        TimeScaleEffect timeScaleEffect{};
        bool silent{false};
    };

    // Counters for the dev tools.
    // backendName:     "FMOD x.yy.zz" or "silent".
    // soundCount:      loaded sounds.
    // voiceCount:      voices the AudioSystem tracks (playing or virtual).
    // playingVoices:   voices the backend reports as playing (real and virtual).
    // audibleVoices:   voices actually mixed.
    // dspCpuPercent:   mixer thread CPU use, in percent of one core.
    // streamCpuPercent: stream decoding CPU use, in percent of one core.
    // updateCpuPercent: CPU use of the per-frame update, in percent of one core.
    struct AudioStats
    {
        const char* backendName{"silent"};
        uint32_t soundCount{0};
        uint32_t voiceCount{0};
        uint32_t playingVoices{0};
        uint32_t audibleVoices{0};
        float dspCpuPercent{0.0f};
        float streamCpuPercent{0.0f};
        float updateCpuPercent{0.0f};
    };
} //namespace lockstep::audio

#endif //LOCKSTEP_AUDIO_AUDIO_TYPES_HPP
