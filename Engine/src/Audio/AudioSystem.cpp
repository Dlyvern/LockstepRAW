#include "Lockstep/Audio/AudioSystem.hpp"

#include "Lockstep/Assets/FileSystem.hpp"

#include "AudioBackend.hpp"
#include "AudioMix.hpp"

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace lockstep::audio
{
    namespace
    {
        constexpr uint32_t k_maxChannels{2};
        constexpr uint32_t k_minSampleRate{8000};
        constexpr uint32_t k_maxSampleRate{192000};

        std::size_t busIndex(Bus bus)
        {
            return static_cast<std::size_t>(bus);
        }

        bool isValidBus(Bus bus)
        {
            return busIndex(bus) < k_busCount;
        }

        Status validate(const SoundDescription& description)
        {
            const bool distancesValid = std::isfinite(description.minDistance) && std::isfinite(description.maxDistance)
                && description.minDistance > 0.0f && description.maxDistance >= description.minDistance;

            if (description.is3d && !distancesValid)
            {
                return Error{ErrorCode::InvalidArgument, "audio: 3D sounds need 0 < minDistance <= maxDistance"};
            }

            return {};
        }

        float clampBusValue(float volume)
        {
            return std::isfinite(volume) ? std::clamp(volume, 0.0f, k_maxBusValue) : 0.0f;
        }

        float clampPitch(float pitch)
        {
            return std::isfinite(pitch) ? std::clamp(pitch, k_minPitch, k_maxBusValue) : 1.0f;
        }

        struct BusState
        {
            float volume{1.0f};
            float timeScale{1.0f};
            float smoothedScale{1.0f};
            TimeScaleEffect effect{.followsPitch = false, .lowPass = false};
        };
    } //namespace

    struct AudioState
    {
        AudioBackend backend;
        std::array<BusState, k_busCount> buses{};
        std::optional<ListenerPose> listenerOverride{};
        ListenerPose listener{};
        bool hasListener{false};
    };

    Result<AudioSystem> AudioSystem::create(const AudioDescription& description)
    {
        auto backend = AudioBackend::create(description);

        if (!backend)
        {
            return backend.error();
        }

        auto state = std::make_unique<AudioState>(AudioState{.backend = std::move(backend).value()});
        state->buses[busIndex(Bus::World)].effect = description.timeScaleEffect;

        for (std::size_t index = 0; index < k_busCount; ++index)
        {
            const BusState& bus = state->buses[index];
            state->backend.applyBus(static_cast<Bus>(index), busMix(bus.volume, bus.smoothedScale, bus.effect));
        }

        return AudioSystem(std::move(state));
    }

    AudioSystem::AudioSystem(std::unique_ptr<AudioState> state) noexcept
        : m_state(std::move(state))
    {
    }

    AudioSystem::~AudioSystem() = default;

    AudioSystem::AudioSystem(AudioSystem&& other) noexcept = default;

    AudioSystem& AudioSystem::operator=(AudioSystem&& other) noexcept = default;

    Result<SoundHandle> AudioSystem::loadSound(const assets::FileSystem& files, std::string_view path,
        const SoundDescription& description)
    {
        if (auto valid = validate(description); !valid)
        {
            return valid.error();
        }

        auto bytes = files.read(path);

        if (!bytes)
        {
            return bytes.error();
        }

        auto sound = m_state->backend.createSound(std::move(bytes).value(), description);

        if (!sound)
        {
            return Error{sound.error().code, "audio: loading '" + std::string(path) + "': " + sound.error().message};
        }

        return sound;
    }

    Result<SoundHandle> AudioSystem::createSound(std::span<const std::byte> encoded, const SoundDescription& description)
    {
        if (encoded.empty())
        {
            return Error{ErrorCode::InvalidArgument, "audio: createSound needs data"};
        }

        if (auto valid = validate(description); !valid)
        {
            return valid.error();
        }

        return m_state->backend.createSound(std::vector<std::byte>(encoded.begin(), encoded.end()), description);
    }

    Result<SoundHandle> AudioSystem::createSoundFromSamples(std::span<const float> samples, uint32_t sampleRate,
        uint32_t channels, const SoundDescription& description)
    {
        if (channels == 0 || channels > k_maxChannels || samples.empty() || samples.size() % channels != 0)
        {
            return Error{ErrorCode::InvalidArgument, "audio: samples need 1 or 2 channels and whole frames"};
        }

        if (sampleRate < k_minSampleRate || sampleRate > k_maxSampleRate)
        {
            return Error{ErrorCode::InvalidArgument, "audio: sample rate must be within 8000-192000 Hz"};
        }

        if (auto valid = validate(description); !valid)
        {
            return valid.error();
        }

        SoundDescription sampled = description;
        sampled.isStreamed = false;

        return m_state->backend.createSoundFromSamples(
            PcmData{.samples = samples, .sampleRate = sampleRate, .channels = channels}, sampled);
    }

    bool AudioSystem::destroySound(SoundHandle sound)
    {
        return m_state && m_state->backend.destroySound(sound);
    }

    bool AudioSystem::isAlive(SoundHandle sound) const noexcept
    {
        return m_state && m_state->backend.isAlive(sound);
    }

    Result<VoiceHandle> AudioSystem::play(SoundHandle sound, const PlayDescription& description)
    {
        if (!m_state->backend.isAlive(sound))
        {
            return Error{ErrorCode::InvalidHandle, "audio: play with a stale sound"};
        }

        if (!isValidBus(description.bus))
        {
            return Error{ErrorCode::InvalidArgument, "audio: play on an unknown bus"};
        }

        PlayDescription clamped = description;
        clamped.volume = clampBusValue(description.volume);
        clamped.pitch = clampPitch(description.pitch);

        return m_state->backend.play(sound, clamped, m_state->listener.position);
    }

    bool AudioSystem::stop(VoiceHandle voice)
    {
        return m_state && m_state->backend.stop(voice);
    }

    bool AudioSystem::isPlaying(VoiceHandle voice) const
    {
        return m_state && m_state->backend.isPlaying(voice);
    }

    bool AudioSystem::setVoicePosition(VoiceHandle voice, const math::Vec3& position, const math::Vec3& velocity)
    {
        return m_state && m_state->backend.setVoicePosition(voice, position, velocity);
    }

    bool AudioSystem::setVoiceVolume(VoiceHandle voice, float volume)
    {
        return m_state && m_state->backend.setVoiceVolume(voice, clampBusValue(volume));
    }

    bool AudioSystem::setVoicePitch(VoiceHandle voice, float pitch)
    {
        return m_state && m_state->backend.setVoicePitch(voice, clampPitch(pitch));
    }

    void AudioSystem::setBusVolume(Bus bus, float volume)
    {
        if (isValidBus(bus))
        {
            m_state->buses[busIndex(bus)].volume = clampBusValue(volume);
        }
    }

    float AudioSystem::busVolume(Bus bus) const
    {
        return isValidBus(bus) ? m_state->buses[busIndex(bus)].volume : 0.0f;
    }

    void AudioSystem::setBusTimeScale(Bus bus, float scale)
    {
        if (bus == Bus::Player)
        {
            assert(false && "the Player bus is locked to time scale 1");
            return;
        }

        if (isValidBus(bus))
        {
            m_state->buses[busIndex(bus)].timeScale = clampBusValue(scale);
        }
    }

    float AudioSystem::busTimeScale(Bus bus) const
    {
        return isValidBus(bus) ? m_state->buses[busIndex(bus)].timeScale : 1.0f;
    }

    void AudioSystem::setTimeScaleEffect(Bus bus, const TimeScaleEffect& effect)
    {
        if (isValidBus(bus))
        {
            m_state->buses[busIndex(bus)].effect = effect;
        }
    }

    const TimeScaleEffect& AudioSystem::timeScaleEffect(Bus bus) const
    {
        return m_state->buses[isValidBus(bus) ? busIndex(bus) : busIndex(Bus::Master)].effect;
    }

    void AudioSystem::setListener(const ListenerPose& pose)
    {
        m_state->listenerOverride = pose;
    }

    void AudioSystem::update(float frameSeconds, const ListenerPose& cameraPose)
    {
        AudioState& state = *m_state;

        for (std::size_t index = 0; index < k_busCount; ++index)
        {
            BusState& bus = state.buses[index];
            bus.smoothedScale = smoothTowards(bus.smoothedScale, bus.timeScale, frameSeconds, bus.effect.smoothingSeconds);
            state.backend.applyBus(static_cast<Bus>(index), busMix(bus.volume, bus.smoothedScale, bus.effect));
        }

        const ListenerPose pose = state.listenerOverride.value_or(cameraPose);
        const math::Vec3 velocity = state.hasListener
            ? listenerVelocity(state.listener.position, pose.position, frameSeconds)
            : math::Vec3{0.0f, 0.0f, 0.0f};

        state.listener = pose;
        state.hasListener = true;
        state.listenerOverride.reset();
        state.backend.update(pose, velocity);
    }

    AudioStats AudioSystem::stats() const
    {
        return m_state ? m_state->backend.stats() : AudioStats{};
    }
} //namespace lockstep::audio
