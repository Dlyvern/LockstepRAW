#ifndef LOCKSTEP_DEBUG_AUDIO_PANEL_HPP
#define LOCKSTEP_DEBUG_AUDIO_PANEL_HPP

#include "Lockstep/Audio/AudioSystem.hpp"

#include <string>

namespace lockstep::debug
{
    // What the Audio panel remembers between frames: its test tones (created on first use, and
    // released with the AudioSystem) and the last error from a button.
    struct AudioPanelState
    {
        audio::SoundHandle tone2d{};
        audio::SoundHandle tone3d{};
        std::string lastError;
    };

    // Draws the "Audio" ImGui window: backend, sound and voice counts, mixer CPU use, a volume
    // slider per bus, the World bus' time scale and its bullet-time effect settings, and test
    // tone buttons (a 2D tone on the Ui bus, a 3D tone at the listener on the World bus, which
    // follows bullet-time). Must be called inside an ImGui frame.
    // panel: the panel's remembered state.
    // audio: the audio system to show and control.
    void drawAudioPanel(AudioPanelState& panel, audio::AudioSystem& audio);
} //namespace lockstep::debug

#endif //LOCKSTEP_DEBUG_AUDIO_PANEL_HPP
