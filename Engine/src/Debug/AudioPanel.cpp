#include "AudioPanel.hpp"

#include <imgui.h>

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace lockstep::debug
{
    namespace
    {
        constexpr ImVec2 k_panelPosition{10.0f, 470.0f};
        constexpr ImVec4 k_errorColor{1.0f, 0.4f, 0.4f, 1.0f};
        constexpr std::array<const char*, audio::k_busCount> k_busLabels{"Master", "Music", "Ambience", "World", "Player", "Ui"};
        constexpr uint32_t k_toneSampleRate{48000};
        constexpr float k_toneSeconds{0.6f};
        constexpr float k_toneFadeSeconds{0.01f};
        constexpr float k_toneFrequency{440.0f};
        constexpr float k_toneAmplitude{0.3f};
        constexpr float k_maxSliderVolume{2.0f};
        constexpr float k_minCutoffHz{100.0f};
        constexpr float k_maxCutoffHz{5000.0f};
        constexpr float k_maxSmoothingSeconds{1.0f};

        std::vector<float> sineTone()
        {
            const std::size_t sampleCount = static_cast<std::size_t>(k_toneSeconds * static_cast<float>(k_toneSampleRate));
            const float fadeSamples = k_toneFadeSeconds * static_cast<float>(k_toneSampleRate);
            std::vector<float> samples(sampleCount);

            for (std::size_t index = 0; index < sampleCount; ++index)
            {
                const float sample = static_cast<float>(index);
                const float remaining = static_cast<float>(sampleCount - index);
                const float envelope = std::fmin(1.0f, std::fmin(sample, remaining) / fadeSamples);
                const float phase = 2.0f * math::k_pi * k_toneFrequency * sample / static_cast<float>(k_toneSampleRate);
                samples[index] = k_toneAmplitude * envelope * std::sin(phase);
            }

            return samples;
        }

        void playTone(AudioPanelState& panel, audio::AudioSystem& audio, audio::SoundHandle& tone, bool is3d)
        {
            if (!audio.isAlive(tone))
            {
                const std::vector<float> samples = sineTone();
                auto created = audio.createSoundFromSamples(samples, k_toneSampleRate, 1,
                    audio::SoundDescription{.is3d = is3d});

                if (!created)
                {
                    panel.lastError = created.error().message;
                    return;
                }

                tone = created.value();
            }

            auto voice = audio.play(tone, audio::PlayDescription{.bus = is3d ? audio::Bus::World : audio::Bus::Ui});
            panel.lastError = voice ? std::string() : voice.error().message;
        }

        void drawStats(const audio::AudioSystem& audio)
        {
            const audio::AudioStats stats = audio.stats();

            ImGui::Text("Backend     %s", stats.backendName);
            ImGui::Text("Sounds      %u", stats.soundCount);
            ImGui::Text("Voices      %u tracked, %u playing, %u audible", stats.voiceCount, stats.playingVoices,
                stats.audibleVoices);
            ImGui::Text("CPU         %.1f %% mix, %.1f %% stream, %.1f %% update", stats.dspCpuPercent,
                stats.streamCpuPercent, stats.updateCpuPercent);
        }

        void drawBuses(audio::AudioSystem& audio)
        {
            for (std::size_t index = 0; index < audio::k_busCount; ++index)
            {
                const audio::Bus bus = static_cast<audio::Bus>(index);
                float volume = audio.busVolume(bus);

                if (ImGui::SliderFloat(k_busLabels[index], &volume, 0.0f, k_maxSliderVolume, "%.2f"))
                {
                    audio.setBusVolume(bus, volume);
                }
            }
        }

        void drawWorldTimeScale(audio::AudioSystem& audio)
        {
            audio::TimeScaleEffect effect = audio.timeScaleEffect(audio::Bus::World);

            ImGui::Text("World time scale %.2f", audio.busTimeScale(audio::Bus::World));
            bool changed = ImGui::Checkbox("Pitch follows scale", &effect.followsPitch);
            changed |= ImGui::Checkbox("Low-pass", &effect.lowPass);
            changed |= ImGui::SliderFloat("Cutoff at 0 (Hz)", &effect.lowPassCutoffHz, k_minCutoffHz, k_maxCutoffHz, "%.0f",
                ImGuiSliderFlags_Logarithmic);
            changed |= ImGui::SliderFloat("Smoothing (s)", &effect.smoothingSeconds, 0.0f, k_maxSmoothingSeconds, "%.2f");

            if (changed)
            {
                audio.setTimeScaleEffect(audio::Bus::World, effect);
            }
        }
    } //namespace

    void drawAudioPanel(AudioPanelState& panel, audio::AudioSystem& audio)
    {
        ImGui::SetNextWindowPos(k_panelPosition, ImGuiCond_FirstUseEver);

        if (ImGui::Begin("Audio", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            drawStats(audio);
            ImGui::SeparatorText("Bus volumes");
            drawBuses(audio);
            ImGui::SeparatorText("Bullet-time (World bus)");
            drawWorldTimeScale(audio);
            ImGui::SeparatorText("Test");

            if (ImGui::Button("Tone (2D, Ui)"))
            {
                playTone(panel, audio, panel.tone2d, false);
            }

            ImGui::SameLine();

            if (ImGui::Button("Tone (3D, World)"))
            {
                playTone(panel, audio, panel.tone3d, true);
            }

            if (!panel.lastError.empty())
            {
                ImGui::TextColored(k_errorColor, "%s", panel.lastError.c_str());
            }
        }

        ImGui::End();
    }
} //namespace lockstep::debug
