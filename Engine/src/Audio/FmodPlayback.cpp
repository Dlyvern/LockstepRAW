#include "FmodState.hpp"

#include <limits>
#include <optional>
#include <utility>

namespace lockstep::audio
{
    namespace
    {
        FMOD_MODE soundMode(const SoundDescription& description)
        {
            FMOD_MODE mode = description.is3d ? (FMOD_3D | FMOD_3D_INVERSEROLLOFF) : FMOD_2D;
            mode |= description.isLooping ? FMOD_LOOP_NORMAL : FMOD_LOOP_OFF;

            return mode;
        }

        Result<SoundHandle> addSound(AudioBackendState& state, FMOD::Sound* sound, std::vector<std::byte> streamBytes,
            const SoundDescription& description)
        {
            if (description.is3d)
            {
                if (FMOD_RESULT result = sound->set3DMinMaxDistance(description.minDistance, description.maxDistance);
                    result != FMOD_OK)
                {
                    sound->release();
                    return fmodError(result, "setting sound distances");
                }
            }

            auto key = state.sounds.insert(FmodSound{.sound = sound, .streamBytes = std::move(streamBytes)});

            if (!key)
            {
                sound->release();
                return Error{ErrorCode::OutOfMemory, "audio: too many sounds"};
            }

            return SoundHandle{.index = key->index, .generation = key->generation};
        }

        FMOD::Channel* findChannel(const AudioBackendState& state, VoiceHandle voice)
        {
            FMOD::Channel* const* channel = state.voices.find(voiceKey(voice));

            return channel != nullptr ? *channel : nullptr;
        }

        Status startChannel(FMOD::Channel& channel, const FmodSound& sound, const PlayDescription& description,
            const math::Vec3& listener)
        {
            FMOD_MODE mode = 0;

            if (FMOD_RESULT result = sound.sound->getMode(&mode); result != FMOD_OK)
            {
                return fmodError(result, "reading a sound's mode");
            }

            FMOD_RESULT result = channel.setVolume(description.volume);
            result = result == FMOD_OK ? channel.setPitch(description.pitch) : result;

            if (result == FMOD_OK && (mode & FMOD_3D) != 0)
            {
                const FMOD_VECTOR position = toFmod(description.position.value_or(listener));
                const FMOD_VECTOR velocity = toFmod(description.velocity);
                result = channel.set3DAttributes(&position, &velocity);
            }

            result = result == FMOD_OK ? channel.setPaused(false) : result;

            if (result != FMOD_OK)
            {
                return fmodError(result, "starting a voice");
            }

            return {};
        }
    } //namespace

    Result<SoundHandle> AudioBackend::createSound(std::vector<std::byte> encoded, const SoundDescription& description)
    {
        if (encoded.size() > std::numeric_limits<unsigned int>::max())
        {
            return Error{ErrorCode::AssetLoadFailed, "audio: sound file too large"};
        }

        FMOD_MODE mode = soundMode(description);
        mode |= description.isStreamed ? (FMOD_CREATESTREAM | FMOD_OPENMEMORY_POINT) : (FMOD_CREATESAMPLE | FMOD_OPENMEMORY);

        FMOD_CREATESOUNDEXINFO info{};
        info.cbsize = sizeof(info);
        info.length = static_cast<unsigned int>(encoded.size());

        FMOD::Sound* sound = nullptr;

        if (FMOD_RESULT result = m_state->system->createSound(reinterpret_cast<const char*>(encoded.data()), mode, &info,
                &sound); result != FMOD_OK)
        {
            return fmodError(result, "decoding a sound", ErrorCode::AssetLoadFailed);
        }

        if (!description.isStreamed)
        {
            encoded = {};
        }

        return addSound(*m_state, sound, std::move(encoded), description);
    }

    Result<SoundHandle> AudioBackend::createSoundFromSamples(const PcmData& pcm, const SoundDescription& description)
    {
        const std::size_t byteCount = pcm.samples.size_bytes();

        if (byteCount > std::numeric_limits<unsigned int>::max())
        {
            return Error{ErrorCode::InvalidArgument, "audio: too many samples"};
        }

        const FMOD_MODE mode = soundMode(description) | FMOD_OPENMEMORY | FMOD_OPENRAW | FMOD_CREATESAMPLE;

        FMOD_CREATESOUNDEXINFO info{};
        info.cbsize = sizeof(info);
        info.length = static_cast<unsigned int>(byteCount);
        info.numchannels = static_cast<int>(pcm.channels);
        info.defaultfrequency = static_cast<int>(pcm.sampleRate);
        info.format = FMOD_SOUND_FORMAT_PCMFLOAT;

        FMOD::Sound* sound = nullptr;

        if (FMOD_RESULT result = m_state->system->createSound(reinterpret_cast<const char*>(pcm.samples.data()), mode,
                &info, &sound); result != FMOD_OK)
        {
            return fmodError(result, "creating a sound from samples");
        }

        return addSound(*m_state, sound, {}, description);
    }

    bool AudioBackend::destroySound(SoundHandle sound)
    {
        if (!m_state)
        {
            return false;
        }

        std::optional<FmodSound> removed = m_state->sounds.remove(soundKey(sound));

        if (!removed)
        {
            return false;
        }

        logFailure(*m_state, removed->sound->release(), "releasing a sound");

        return true;
    }

    bool AudioBackend::isAlive(SoundHandle sound) const noexcept
    {
        return m_state && m_state->sounds.find(soundKey(sound)) != nullptr;
    }

    Result<VoiceHandle> AudioBackend::play(SoundHandle sound, const PlayDescription& description, const math::Vec3& listener)
    {
        const FmodSound* loaded = m_state->sounds.find(soundKey(sound));
        FMOD::ChannelGroup* bus = m_state->buses[static_cast<std::size_t>(description.bus)];
        FMOD::Channel* channel = nullptr;

        if (FMOD_RESULT result = m_state->system->playSound(loaded->sound, bus, true, &channel); result != FMOD_OK)
        {
            return fmodError(result, "playing a sound");
        }

        if (auto started = startChannel(*channel, *loaded, description, listener); !started)
        {
            channel->stop();
            return started.error();
        }

        auto key = m_state->voices.insert(channel);

        if (!key)
        {
            channel->stop();
            return Error{ErrorCode::OutOfMemory, "audio: too many voices"};
        }

        return VoiceHandle{.index = key->index, .generation = key->generation};
    }

    bool AudioBackend::stop(VoiceHandle voice)
    {
        if (!m_state)
        {
            return false;
        }

        const bool wasPlaying = isPlaying(voice);
        std::optional<FMOD::Channel*> removed = m_state->voices.remove(voiceKey(voice));

        if (removed && wasPlaying)
        {
            logFailure(*m_state, (*removed)->stop(), "stopping a voice");
        }

        return wasPlaying;
    }

    bool AudioBackend::isPlaying(VoiceHandle voice) const
    {
        FMOD::Channel* channel = m_state ? findChannel(*m_state, voice) : nullptr;
        bool isPlaying = false;

        return channel != nullptr && channel->isPlaying(&isPlaying) == FMOD_OK && isPlaying;
    }

    bool AudioBackend::setVoicePosition(VoiceHandle voice, const math::Vec3& position, const math::Vec3& velocity)
    {
        FMOD::Channel* channel = m_state ? findChannel(*m_state, voice) : nullptr;
        const FMOD_VECTOR fmodPosition = toFmod(position);
        const FMOD_VECTOR fmodVelocity = toFmod(velocity);

        return channel != nullptr && channel->set3DAttributes(&fmodPosition, &fmodVelocity) == FMOD_OK;
    }

    bool AudioBackend::setVoiceVolume(VoiceHandle voice, float volume)
    {
        FMOD::Channel* channel = m_state ? findChannel(*m_state, voice) : nullptr;

        return channel != nullptr && channel->setVolume(volume) == FMOD_OK;
    }

    bool AudioBackend::setVoicePitch(VoiceHandle voice, float pitch)
    {
        FMOD::Channel* channel = m_state ? findChannel(*m_state, voice) : nullptr;

        return channel != nullptr && channel->setPitch(pitch) == FMOD_OK;
    }
} //namespace lockstep::audio
