#ifndef LOCKSTEP_AUDIO_AUDIO_BACKEND_HPP
#define LOCKSTEP_AUDIO_AUDIO_BACKEND_HPP

#include "Lockstep/Audio/AudioTypes.hpp"
#include "Lockstep/Core/Result.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <vector>

namespace lockstep::audio
{
    struct AudioBackendState;

    // What one bus applies this frame, computed by the AudioSystem from its volume, time scale
    // and TimeScaleEffect.
    // volume:          linear gain.
    // pitch:           playback-rate multiplier for every voice on the bus.
    // lowPassCutoffHz: low-pass cutoff; 0 leaves the filter open (bypassed).
    // isPaused:        true while the bus' time scale is (nearly) 0.
    struct BusMix
    {
        float volume{1.0f};
        float pitch{1.0f};
        float lowPassCutoffHz{0.0f};
        bool isPaused{false};
    };

    // Raw PCM data for AudioBackend::createSoundFromSamples().
    // samples:    interleaved 32-bit float samples.
    // sampleRate: samples per second per channel.
    // channels:   1 or 2.
    struct PcmData
    {
        std::span<const float> samples;
        uint32_t sampleRate{0};
        uint32_t channels{0};
    };

    // The library behind AudioSystem: FMOD (FmodBackend.cpp) or silence (SilentBackend.cpp),
    // chosen at build time by LOCKSTEP_WITH_FMOD. It owns sounds and voices and their handles;
    // the AudioSystem validates arguments and computes the bus mix and listener motion.
    // Arguments reaching it are already validated.
    class AudioBackend
    {
    public:
        // Starts the backend and creates the bus hierarchy.
        // description: voice limit, Doppler scale and whether to stay silent.
        // Returns:     the backend, or an error if the audio library can't start.
        [[nodiscard]] static Result<AudioBackend> create(const AudioDescription& description);

        // Releases every voice and sound, then the library.
        ~AudioBackend();

        AudioBackend(AudioBackend&& other) noexcept;
        AudioBackend& operator=(AudioBackend&& other) noexcept;
        AudioBackend(const AudioBackend&) = delete;
        AudioBackend& operator=(const AudioBackend&) = delete;

        // Creates a sound from an encoded file.
        // encoded:     the file's contents; kept alive by the backend for streamed sounds.
        // description: 2D/3D, looping, streaming and distances.
        // Returns:     the sound, or AssetLoadFailed if it can't be decoded.
        [[nodiscard]] Result<SoundHandle> createSound(std::vector<std::byte> encoded, const SoundDescription& description);

        // Creates a sound from PCM samples (copied).
        // pcm:         the samples and their format.
        // description: 2D/3D, looping and distances.
        // Returns:     the sound, or an error the library reported.
        [[nodiscard]] Result<SoundHandle> createSoundFromSamples(const PcmData& pcm, const SoundDescription& description);

        // Releases a sound and stops its voices.
        // sound:   the sound.
        // Returns: true if it was alive.
        bool destroySound(SoundHandle sound);

        // sound:   the sound.
        // Returns: true if it is loaded.
        [[nodiscard]] bool isAlive(SoundHandle sound) const noexcept;

        // Starts a voice.
        // sound:       a living sound.
        // description: bus, volume, pitch (already clamped) and position.
        // listener:    where a 3D voice without a position starts.
        // Returns:     the voice, or an error the library reported.
        [[nodiscard]] Result<VoiceHandle> play(SoundHandle sound, const PlayDescription& description,
            const math::Vec3& listener);

        // voice:   the voice.
        // Returns: true if it was playing and is now stopped.
        bool stop(VoiceHandle voice);

        // voice:   the voice.
        // Returns: true while it plays (or is virtual).
        [[nodiscard]] bool isPlaying(VoiceHandle voice) const;

        // voice:    the voice.
        // position: world position.
        // velocity: m/s.
        // Returns:  false for a stale voice.
        bool setVoicePosition(VoiceHandle voice, const math::Vec3& position, const math::Vec3& velocity);

        // voice:   the voice.
        // volume:  linear gain, already clamped.
        // Returns: false for a stale voice.
        bool setVoiceVolume(VoiceHandle voice, float volume);

        // voice:   the voice.
        // pitch:   playback rate, already clamped.
        // Returns: false for a stale voice.
        bool setVoicePitch(VoiceHandle voice, float pitch);

        // Applies one bus' mix.
        // bus: the bus.
        // mix: volume, pitch, low-pass and pause state.
        void applyBus(Bus bus, const BusMix& mix);

        // Places the listener, frees finished voices and lets the library mix.
        // pose:     listener position and orientation.
        // velocity: listener velocity in m/s.
        void update(const ListenerPose& pose, const math::Vec3& velocity);

        // Returns: counts and CPU use. Voice and sound counts are filled in by the backend.
        [[nodiscard]] AudioStats stats() const;

    private:
        explicit AudioBackend(std::unique_ptr<AudioBackendState> state) noexcept;

        std::unique_ptr<AudioBackendState> m_state;
    };
} //namespace lockstep::audio

#endif //LOCKSTEP_AUDIO_AUDIO_BACKEND_HPP
