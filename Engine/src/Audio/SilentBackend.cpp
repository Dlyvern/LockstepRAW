#include "AudioBackend.hpp"

#include "Core/HandlePool.hpp"
#include "Lockstep/Core/Log.hpp"

#include <cstdint>
#include <limits>
#include <utility>

namespace lockstep::audio
{
    namespace
    {
        struct SilentSound
        {
            bool is3d{false};
        };

        SoundHandle toSoundHandle(HandlePool<SilentSound>::Key key)
        {
            return SoundHandle{.index = key.index, .generation = key.generation};
        }

        HandlePool<SilentSound>::Key toKey(SoundHandle sound)
        {
            return HandlePool<SilentSound>::Key{.index = sound.index, .generation = sound.generation};
        }
    } //namespace

    struct AudioBackendState
    {
        HandlePool<SilentSound> sounds;
        uint32_t nextVoiceGeneration{1};
    };

    Result<AudioBackend> AudioBackend::create(const AudioDescription&)
    {
        log(LogLevel::Info, "Audio", "built without FMOD: audio is silent");

        return AudioBackend(std::make_unique<AudioBackendState>());
    }

    AudioBackend::AudioBackend(std::unique_ptr<AudioBackendState> state) noexcept
        : m_state(std::move(state))
    {
    }

    AudioBackend::~AudioBackend() = default;

    AudioBackend::AudioBackend(AudioBackend&& other) noexcept = default;

    AudioBackend& AudioBackend::operator=(AudioBackend&& other) noexcept = default;

    Result<SoundHandle> AudioBackend::createSound(std::vector<std::byte>, const SoundDescription& description)
    {
        auto key = m_state->sounds.insert(SilentSound{.is3d = description.is3d});

        if (!key)
        {
            return Error{ErrorCode::OutOfMemory, "audio: too many sounds"};
        }

        return toSoundHandle(*key);
    }

    Result<SoundHandle> AudioBackend::createSoundFromSamples(const PcmData&, const SoundDescription& description)
    {
        return createSound({}, description);
    }

    bool AudioBackend::destroySound(SoundHandle sound)
    {
        return m_state && m_state->sounds.remove(toKey(sound)).has_value();
    }

    bool AudioBackend::isAlive(SoundHandle sound) const noexcept
    {
        return m_state && m_state->sounds.find(toKey(sound)) != nullptr;
    }

    Result<VoiceHandle> AudioBackend::play(SoundHandle, const PlayDescription&, const math::Vec3&)
    {
        const uint32_t generation = m_state->nextVoiceGeneration;
        m_state->nextVoiceGeneration = generation == std::numeric_limits<uint32_t>::max() ? 1 : generation + 1;

        return VoiceHandle{.index = 0, .generation = generation};
    }

    bool AudioBackend::stop(VoiceHandle)
    {
        return false;
    }

    bool AudioBackend::isPlaying(VoiceHandle) const
    {
        return false;
    }

    bool AudioBackend::setVoicePosition(VoiceHandle, const math::Vec3&, const math::Vec3&)
    {
        return false;
    }

    bool AudioBackend::setVoiceVolume(VoiceHandle, float)
    {
        return false;
    }

    bool AudioBackend::setVoicePitch(VoiceHandle, float)
    {
        return false;
    }

    void AudioBackend::applyBus(Bus, const BusMix&)
    {
    }

    void AudioBackend::update(const ListenerPose&, const math::Vec3&)
    {
    }

    AudioStats AudioBackend::stats() const
    {
        AudioStats stats{};
        stats.soundCount = m_state ? m_state->sounds.count() : 0;

        return stats;
    }
} //namespace lockstep::audio
