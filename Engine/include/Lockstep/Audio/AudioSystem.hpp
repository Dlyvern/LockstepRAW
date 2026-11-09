#ifndef LOCKSTEP_AUDIO_AUDIO_SYSTEM_HPP
#define LOCKSTEP_AUDIO_AUDIO_SYSTEM_HPP

#include "Lockstep/Audio/AudioTypes.hpp"
#include "Lockstep/Core/Result.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string_view>

namespace lockstep::assets
{
    class FileSystem;
} //namespace lockstep::assets

namespace lockstep::audio
{
    struct AudioState;

    // Sound playback: loaded sounds, playing voices, mixer buses and the 3D listener, on the FMOD
    // Core API (FMOD's no-sound output when no output device can be opened; a silent backend
    // that only tracks handles when the Engine is built without LOCKSTEP_WITH_FMOD). Owned by the
    // run loop and reachable through app::EngineContext.
    //
    // Audio is presentation only and never replicated: each machine plays sounds from the state
    // and events it already has. Call it from GameApi::frame (or from fixedTick for one-shot
    // events, keeping in mind that a client re-simulating ticks would play them again).
    //
    // Each frame the run loop calls update() after GameApi::frame: it places the listener on the
    // renderer's camera unless Game called setListener() that frame, gives the World bus the World
    // time-domain scale, and lets the backend mix. Nothing here throws; FMOD errors are returned
    // as errors or, for per-frame calls, logged once per kind.
    class AudioSystem
    {
    public:
        // Starts the audio backend and creates the buses.
        // description: voice limit, Doppler and bullet-time settings.
        // Returns:     the audio system. If no output device works, FMOD falls back to its
        //              no-sound output and logs a warning instead of failing; an error only if
        //              FMOD itself can't start.
        [[nodiscard]] static Result<AudioSystem> create(const AudioDescription& description = {});

        // Stops every voice and releases every sound, then the backend.
        ~AudioSystem();

        // Takes over another audio system. `other` becomes empty and may only be destroyed.
        AudioSystem(AudioSystem&& other) noexcept;

        // Shuts this audio system down, then takes over `other`. `other` becomes empty.
        AudioSystem& operator=(AudioSystem&& other) noexcept;

        AudioSystem(const AudioSystem&) = delete;
        AudioSystem& operator=(const AudioSystem&) = delete;

        // Loads a sound file (WAV, OGG Vorbis, FLAC, MP3, ...) through the virtual file system, so
        // loose folders and paks work alike.
        // files:       the file system to read from.
        // path:        virtual path of the file.
        // description: 2D/3D, looping, streaming and distances.
        // Returns:     the sound, or the file system's error, or AssetLoadFailed if the data
        //              can't be decoded, or InvalidArgument for bad distances.
        [[nodiscard]] Result<SoundHandle> loadSound(const assets::FileSystem& files, std::string_view path,
            const SoundDescription& description = {});

        // Creates a sound from an encoded file already in memory. The bytes are copied.
        // encoded:     the file's contents.
        // description: 2D/3D, looping, streaming and distances.
        // Returns:     the sound, or AssetLoadFailed if the data can't be decoded, or
        //              InvalidArgument for empty data or bad distances.
        [[nodiscard]] Result<SoundHandle> createSound(std::span<const std::byte> encoded,
            const SoundDescription& description = {});

        // Creates a sound from raw PCM samples (procedural sounds, test tones). The samples are
        // copied; streaming is ignored.
        // samples:     interleaved 32-bit float samples in [-1, 1].
        // sampleRate:  samples per second per channel.
        // channels:    1 (mono) or 2 (stereo).
        // description: 2D/3D, looping and distances.
        // Returns:     the sound, or InvalidArgument for no samples, a bad rate, channel count or
        //              sample count, or an error the backend reported.
        [[nodiscard]] Result<SoundHandle> createSoundFromSamples(std::span<const float> samples, uint32_t sampleRate,
            uint32_t channels, const SoundDescription& description = {});

        // Releases a sound. Its voices stop and every handle to it becomes stale.
        // sound:   the sound.
        // Returns: true if it was alive and is now released.
        bool destroySound(SoundHandle sound);

        // sound:   the sound.
        // Returns: true if `sound` refers to a loaded sound.
        [[nodiscard]] bool isAlive(SoundHandle sound) const noexcept;

        // Starts a voice of a sound. One-shot voices free themselves when they finish.
        // sound:       the sound.
        // description: bus, volume, pitch and 3D position.
        // Returns:     the voice, or InvalidHandle for a stale sound, or an error the backend
        //              reported. With the silent backend the voice is valid but never playing.
        [[nodiscard]] Result<VoiceHandle> play(SoundHandle sound, const PlayDescription& description = {});

        // Stops a voice. Its handle becomes stale.
        // voice:   the voice.
        // Returns: true if it was playing.
        bool stop(VoiceHandle voice);

        // voice:   the voice.
        // Returns: true while the voice exists (playing, or virtual because too many play).
        [[nodiscard]] bool isPlaying(VoiceHandle voice) const;

        // Moves a 3D voice, e.g. to follow an entity: Game passes its position every frame.
        // voice:    the voice.
        // position: world position.
        // velocity: velocity in m/s, for Doppler.
        // Returns:  false for a stale voice.
        bool setVoicePosition(VoiceHandle voice, const math::Vec3& position, const math::Vec3& velocity = {});

        // Changes a voice's volume.
        // voice:   the voice.
        // volume:  linear gain, 1 = as recorded; negative values count as 0.
        // Returns: false for a stale voice.
        bool setVoiceVolume(VoiceHandle voice, float volume);

        // Changes a voice's pitch.
        // voice:   the voice.
        // pitch:   playback-rate multiplier; the bus' time scale still multiplies it. Clamped
        //          to a small positive minimum.
        // Returns: false for a stale voice.
        bool setVoicePitch(VoiceHandle voice, float pitch);

        // Sets a bus' volume. A bus' output is also scaled by Master's.
        // bus:    the bus.
        // volume: linear gain, 1 = unchanged; clamped to [0, 4].
        void setBusVolume(Bus bus, float volume);

        // bus:     the bus.
        // Returns: its volume as set.
        [[nodiscard]] float busVolume(Bus bus) const;

        // Sets a bus' time scale, which slows its sounds and closes its low-pass filter as set by
        // its TimeScaleEffect. The run loop sets World's every frame from the World time domain;
        // Player is locked to 1 (setting it asserts and is ignored), like its time domain.
        // bus:   the bus.
        // scale: 1 is normal speed; clamped to [0, 4]. At 0 the bus is paused.
        void setBusTimeScale(Bus bus, float scale);

        // bus:     the bus.
        // Returns: its time scale as set (the effect ramps towards it).
        [[nodiscard]] float busTimeScale(Bus bus) const;

        // Sets how a bus reacts to its time scale.
        // bus:    the bus.
        // effect: pitch, low-pass and smoothing settings.
        void setTimeScaleEffect(Bus bus, const TimeScaleEffect& effect);

        // bus:     the bus.
        // Returns: how it reacts to its time scale.
        [[nodiscard]] const TimeScaleEffect& timeScaleEffect(Bus bus) const;

        // Places the listener for this frame, instead of on the camera. Call from GameApi::frame,
        // every frame it should apply.
        // pose: listener position and orientation. Its velocity is derived from frame to frame.
        void setListener(const ListenerPose& pose);

        // Applies this frame's listener, bus settings and voice changes, and lets the backend
        // update. Called by the run loop once per frame after GameApi::frame; Game does not call
        // it.
        // frameSeconds: wall time since the previous frame (smoothing and listener velocity).
        // cameraPose:   the listener when Game did not call setListener() this frame.
        void update(float frameSeconds, const ListenerPose& cameraPose);

        // Returns: loaded sounds, voices, backend name and CPU use.
        [[nodiscard]] AudioStats stats() const;

    private:
        explicit AudioSystem(std::unique_ptr<AudioState> state) noexcept;

        std::unique_ptr<AudioState> m_state;
    };
} //namespace lockstep::audio

#endif //LOCKSTEP_AUDIO_AUDIO_SYSTEM_HPP
