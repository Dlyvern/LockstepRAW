#ifndef LOCKSTEP_AUDIO_FMOD_STATE_HPP
#define LOCKSTEP_AUDIO_FMOD_STATE_HPP

#include "AudioBackend.hpp"

#include "Core/HandlePool.hpp"
#include "Lockstep/Core/Result.hpp"

#include <fmod.hpp>

#include <array>
#include <cstddef>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

namespace lockstep::audio
{
    // One loaded FMOD sound.
    // sound:       the FMOD sound.
    // streamBytes: the encoded file a streamed sound reads from (FMOD_OPENMEMORY_POINT); empty
    //              for sounds decoded up front.
    struct FmodSound
    {
        FMOD::Sound* sound{nullptr};
        std::vector<std::byte> streamBytes;
    };

    // Everything the FMOD backend owns. Its destructor stops every voice and releases sounds,
    // bus filters, buses and the FMOD system, in that order.
    // system:       the FMOD Core system.
    // buses:        one channel group per Bus; Master is FMOD's master group (owned by FMOD).
    // lowPass:      one multiband EQ per bus (band A as a 24 dB low-pass), bypassed while open.
    // sounds:       loaded sounds by SoundHandle.
    // voices:       started channels by VoiceHandle; finished ones are removed in update().
    // backendName:  "FMOD <version>" for AudioStats.
    // loggedErrors: FMOD error codes already logged by per-frame calls, so each logs once.
    struct AudioBackendState
    {
        AudioBackendState() = default;
        ~AudioBackendState();

        AudioBackendState(const AudioBackendState&) = delete;
        AudioBackendState& operator=(const AudioBackendState&) = delete;
        AudioBackendState(AudioBackendState&&) = delete;
        AudioBackendState& operator=(AudioBackendState&&) = delete;

        FMOD::System* system{nullptr};
        std::array<FMOD::ChannelGroup*, k_busCount> buses{};
        std::array<FMOD::DSP*, k_busCount> lowPass{};
        HandlePool<FmodSound> sounds;
        HandlePool<FMOD::Channel*> voices;
        std::string backendName{"FMOD"};
        std::unordered_set<int> loggedErrors;
    };

    // result:  an FMOD result code.
    // what:    the operation that failed, for the message.
    // code:    the Engine error category to report.
    // Returns: an Error naming the operation and FMOD's description of the result.
    [[nodiscard]] Error fmodError(FMOD_RESULT result, std::string_view what, ErrorCode code = ErrorCode::PlatformFailure);

    // Logs a failed per-frame FMOD call as a warning, once per FMOD error code.
    // state:  the backend, which remembers what it has logged.
    // result: the call's result; FMOD_OK logs nothing.
    // what:   the operation, for the message.
    void logFailure(AudioBackendState& state, FMOD_RESULT result, std::string_view what);

    // Converts a vector to FMOD's layout.
    // vector:  an Engine vector.
    // Returns: the same vector in FMOD's layout.
    [[nodiscard]] FMOD_VECTOR toFmod(const math::Vec3& vector);

    // sound:   a sound handle.
    // Returns: its key in AudioBackendState::sounds.
    [[nodiscard]] HandlePool<FmodSound>::Key soundKey(SoundHandle sound);

    // voice:   a voice handle.
    // Returns: its key in AudioBackendState::voices.
    [[nodiscard]] HandlePool<FMOD::Channel*>::Key voiceKey(VoiceHandle voice);
} //namespace lockstep::audio

#endif //LOCKSTEP_AUDIO_FMOD_STATE_HPP
