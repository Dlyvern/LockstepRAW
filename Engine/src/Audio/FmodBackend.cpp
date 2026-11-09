#include "FmodState.hpp"

#include "Lockstep/Core/Log.hpp"

#include <fmod_errors.h>

#include <array>
#include <cmath>
#include <cstdio>
#include <string>
#include <utility>

namespace lockstep::audio
{
    namespace
    {
        constexpr std::array<const char*, k_busCount> k_busNames{"Master", "Music", "Ambience", "World", "Player", "Ui"};
        constexpr float k_distanceFactor{1.0f};
        constexpr float k_rolloffScale{1.0f};
        constexpr float k_minVectorLengthSquared{1.0e-8f};
        constexpr int k_driverNameLength{256};

        std::string versionName(unsigned int version)
        {
            char name[32]{};
            std::snprintf(name, sizeof(name), "FMOD %x.%02x.%02x", (version >> 16) & 0xFFFFu, (version >> 8) & 0xFFu,
                version & 0xFFu);

            return name;
        }

        Status initSystem(FMOD::System& system, const AudioDescription& description)
        {
            const int maxVoices = static_cast<int>(description.maxVoices);

            if (description.silent)
            {
                if (FMOD_RESULT result = system.setOutput(FMOD_OUTPUTTYPE_NOSOUND); result != FMOD_OK)
                {
                    return fmodError(result, "selecting the no-sound output");
                }
            }

            FMOD_RESULT result = system.init(maxVoices, FMOD_INIT_3D_RIGHTHANDED, nullptr);

            if (result != FMOD_OK && !description.silent)
            {
                log(LogLevel::Warning, "Audio", std::string("no audio output (") + FMOD_ErrorString(result)
                    + "), continuing silently");

                if (FMOD_RESULT output = system.setOutput(FMOD_OUTPUTTYPE_NOSOUND); output != FMOD_OK)
                {
                    return fmodError(output, "selecting the no-sound output");
                }

                result = system.init(maxVoices, FMOD_INIT_3D_RIGHTHANDED, nullptr);
            }

            if (result != FMOD_OK)
            {
                return fmodError(result, "initialising FMOD");
            }

            if (FMOD_RESULT settings = system.set3DSettings(description.dopplerScale, k_distanceFactor, k_rolloffScale);
                settings != FMOD_OK)
            {
                return fmodError(settings, "setting 3D settings");
            }

            return {};
        }

        Status createLowPass(AudioBackendState& state, std::size_t index)
        {
            FMOD::DSP* dsp = nullptr;

            if (FMOD_RESULT result = state.system->createDSPByType(FMOD_DSP_TYPE_MULTIBAND_EQ, &dsp); result != FMOD_OK)
            {
                return fmodError(result, "creating a bus low-pass");
            }

            state.lowPass[index] = dsp;

            const FMOD_RESULT filter = dsp->setParameterInt(FMOD_DSP_MULTIBAND_EQ_A_FILTER,
                FMOD_DSP_MULTIBAND_EQ_FILTER_LOWPASS_24DB);
            const FMOD_RESULT bypass = filter == FMOD_OK ? dsp->setBypass(true) : filter;
            const FMOD_RESULT added = bypass == FMOD_OK
                ? state.buses[index]->addDSP(FMOD_CHANNELCONTROL_DSP_TAIL, dsp)
                : bypass;

            if (added != FMOD_OK)
            {
                return fmodError(added, "setting up a bus low-pass");
            }

            return {};
        }

        Status createBuses(AudioBackendState& state)
        {
            FMOD::ChannelGroup* master = nullptr;

            if (FMOD_RESULT result = state.system->getMasterChannelGroup(&master); result != FMOD_OK)
            {
                return fmodError(result, "getting the master channel group");
            }

            state.buses[static_cast<std::size_t>(Bus::Master)] = master;

            for (std::size_t index = 0; index < k_busCount; ++index)
            {
                if (index != static_cast<std::size_t>(Bus::Master))
                {
                    FMOD::ChannelGroup* group = nullptr;

                    if (FMOD_RESULT result = state.system->createChannelGroup(k_busNames[index], &group); result != FMOD_OK)
                    {
                        return fmodError(result, "creating a bus");
                    }

                    state.buses[index] = group;

                    if (FMOD_RESULT result = master->addGroup(group); result != FMOD_OK)
                    {
                        return fmodError(result, "routing a bus to master");
                    }
                }

                if (auto lowPass = createLowPass(state, index); !lowPass)
                {
                    return lowPass;
                }
            }

            return {};
        }

        void logOutput(AudioBackendState& state)
        {
            FMOD_OUTPUTTYPE output = FMOD_OUTPUTTYPE_AUTODETECT;
            logFailure(state, state.system->getOutput(&output), "reading the output type");

            if (output == FMOD_OUTPUTTYPE_NOSOUND)
            {
                log(LogLevel::Info, "Audio", state.backendName + ", no-sound output");
                return;
            }

            std::array<char, k_driverNameLength> driver{};
            int sampleRate = 0;
            logFailure(state, state.system->getDriverInfo(0, driver.data(), k_driverNameLength, nullptr, &sampleRate,
                nullptr, nullptr), "reading the output driver");
            log(LogLevel::Info, "Audio", state.backendName + ", output '" + driver.data() + "' at "
                + std::to_string(sampleRate) + " Hz");
        }

        math::Vec3 normalizedOr(const math::Vec3& vector, const math::Vec3& fallback)
        {
            const float lengthSquared = math::lengthSquared(vector);

            return lengthSquared > k_minVectorLengthSquared ? vector * (1.0f / std::sqrt(lengthSquared)) : fallback;
        }

        math::Vec3 anyPerpendicular(const math::Vec3& direction)
        {
            const math::Vec3 axis = std::abs(direction.x) < 0.9f ? math::Vec3{1.0f, 0.0f, 0.0f} : math::Vec3{0.0f, 0.0f, 1.0f};

            return normalizedOr(math::cross(direction, axis), math::Vec3{0.0f, 1.0f, 0.0f});
        }
    } //namespace

    AudioBackendState::~AudioBackendState()
    {
        for (uint32_t index = 0; index < voices.capacity(); ++index)
        {
            if (auto key = voices.keyAt(index))
            {
                (*voices.find(*key))->stop();
            }
        }

        for (uint32_t index = 0; index < sounds.capacity(); ++index)
        {
            if (auto key = sounds.keyAt(index))
            {
                sounds.find(*key)->sound->release();
            }
        }

        for (std::size_t index = 0; index < k_busCount; ++index)
        {
            if (lowPass[index] != nullptr)
            {
                if (buses[index] != nullptr)
                {
                    buses[index]->removeDSP(lowPass[index]);
                }

                lowPass[index]->release();
            }

            if (buses[index] != nullptr && index != static_cast<std::size_t>(Bus::Master))
            {
                buses[index]->release();
            }
        }

        if (system != nullptr)
        {
            system->release();
        }
    }

    Error fmodError(FMOD_RESULT result, std::string_view what, ErrorCode code)
    {
        return Error{code, "audio: " + std::string(what) + " failed: " + FMOD_ErrorString(result)};
    }

    void logFailure(AudioBackendState& state, FMOD_RESULT result, std::string_view what)
    {
        if (result == FMOD_OK || !state.loggedErrors.insert(static_cast<int>(result)).second)
        {
            return;
        }

        log(LogLevel::Warning, "Audio", std::string(what) + " failed: " + FMOD_ErrorString(result));
    }

    FMOD_VECTOR toFmod(const math::Vec3& vector)
    {
        return FMOD_VECTOR{vector.x, vector.y, vector.z};
    }

    HandlePool<FmodSound>::Key soundKey(SoundHandle sound)
    {
        return HandlePool<FmodSound>::Key{.index = sound.index, .generation = sound.generation};
    }

    HandlePool<FMOD::Channel*>::Key voiceKey(VoiceHandle voice)
    {
        return HandlePool<FMOD::Channel*>::Key{.index = voice.index, .generation = voice.generation};
    }

    Result<AudioBackend> AudioBackend::create(const AudioDescription& description)
    {
        auto state = std::make_unique<AudioBackendState>();

        if (FMOD_RESULT result = FMOD::System_Create(&state->system); result != FMOD_OK)
        {
            return fmodError(result, "creating the FMOD system");
        }

        unsigned int version = 0;

        if (FMOD_RESULT result = state->system->getVersion(&version); result != FMOD_OK)
        {
            return fmodError(result, "reading the FMOD version");
        }

        state->backendName = versionName(version);

        if (auto initialised = initSystem(*state->system, description); !initialised)
        {
            return initialised.error();
        }

        if (auto buses = createBuses(*state); !buses)
        {
            return buses.error();
        }

        logOutput(*state);

        return AudioBackend(std::move(state));
    }

    AudioBackend::AudioBackend(std::unique_ptr<AudioBackendState> state) noexcept
        : m_state(std::move(state))
    {
    }

    AudioBackend::~AudioBackend() = default;

    AudioBackend::AudioBackend(AudioBackend&& other) noexcept = default;

    AudioBackend& AudioBackend::operator=(AudioBackend&& other) noexcept = default;

    void AudioBackend::applyBus(Bus bus, const BusMix& mix)
    {
        const std::size_t index = static_cast<std::size_t>(bus);
        FMOD::ChannelGroup* group = m_state->buses[index];
        FMOD::DSP* lowPass = m_state->lowPass[index];
        const bool isOpen = mix.lowPassCutoffHz <= 0.0f;

        logFailure(*m_state, group->setVolume(mix.volume), "setting a bus volume");
        logFailure(*m_state, group->setPitch(mix.pitch), "setting a bus pitch");
        logFailure(*m_state, group->setPaused(mix.isPaused), "pausing a bus");
        logFailure(*m_state, lowPass->setBypass(isOpen), "bypassing a bus low-pass");

        if (!isOpen)
        {
            logFailure(*m_state, lowPass->setParameterFloat(FMOD_DSP_MULTIBAND_EQ_A_FREQUENCY, mix.lowPassCutoffHz),
                "setting a bus low-pass cutoff");
        }
    }

    void AudioBackend::update(const ListenerPose& pose, const math::Vec3& velocity)
    {
        const math::Vec3 forward = normalizedOr(pose.forward, math::Vec3{0.0f, 0.0f, -1.0f});
        const math::Vec3 sideways = math::cross(forward, pose.up);
        const math::Vec3 right = normalizedOr(sideways, anyPerpendicular(forward));
        const math::Vec3 up = normalizedOr(math::cross(right, forward), math::Vec3{0.0f, 1.0f, 0.0f});

        const FMOD_VECTOR position = toFmod(pose.position);
        const FMOD_VECTOR fmodVelocity = toFmod(velocity);
        const FMOD_VECTOR fmodForward = toFmod(forward);
        const FMOD_VECTOR fmodUp = toFmod(up);
        logFailure(*m_state, m_state->system->set3DListenerAttributes(0, &position, &fmodVelocity, &fmodForward, &fmodUp),
            "placing the listener");

        for (uint32_t index = 0; index < m_state->voices.capacity(); ++index)
        {
            const auto key = m_state->voices.keyAt(index);

            if (key && !isPlaying(VoiceHandle{.index = key->index, .generation = key->generation}))
            {
                m_state->voices.remove(*key);
            }
        }

        logFailure(*m_state, m_state->system->update(), "updating FMOD");
    }

    AudioStats AudioBackend::stats() const
    {
        AudioStats stats{};

        if (!m_state)
        {
            return stats;
        }

        int playing = 0;
        int audible = 0;
        FMOD_CPU_USAGE usage{};

        if (m_state->system->getChannelsPlaying(&playing, &audible) == FMOD_OK)
        {
            stats.playingVoices = static_cast<uint32_t>(playing);
            stats.audibleVoices = static_cast<uint32_t>(audible);
        }

        if (m_state->system->getCPUUsage(&usage) == FMOD_OK)
        {
            stats.dspCpuPercent = usage.dsp;
            stats.streamCpuPercent = usage.stream;
            stats.updateCpuPercent = usage.update;
        }

        stats.backendName = m_state->backendName.c_str();
        stats.soundCount = m_state->sounds.count();
        stats.voiceCount = m_state->voices.count();

        return stats;
    }
} //namespace lockstep::audio
