#include "Lights/LightBuzz.hpp"

#include <Lockstep/Assets/FileSystem.hpp>
#include <Lockstep/Audio/AudioSystem.hpp>

#if LOCKSTEP_DEV_TOOLS
#include <imgui.h>
#endif

#include <algorithm>
#include <array>
#include <cstddef>
#include <iostream>
#include <string>
#include <string_view>

namespace game
{
    namespace
    {
        namespace audio = lockstep::audio;

        constexpr std::string_view k_buzzPath{"sounds/ambience/fluorescent_buzz"};
        constexpr std::string_view k_cracklePath{"sounds/ambience/fluorescent_crackle_"};
        constexpr std::array<std::string_view, 2> k_extensions{".ogg", ".wav"};
        constexpr uint32_t k_maxCrackles{8};
        constexpr int k_maxCracklesPerFrame{2};
        constexpr float k_litLevel{0.5f};
        constexpr float k_buzzMinDistance{0.8f};
        constexpr float k_crackleMinDistance{1.0f};
        constexpr int k_voiceLimit{8};

        std::optional<audio::SoundHandle> loadSound(audio::AudioSystem& audioSystem,
            const lockstep::assets::FileSystem& files, const std::string& stem, const audio::SoundDescription& description)
        {
            for (std::string_view extension : k_extensions)
            {
                const std::string path = stem + std::string(extension);

                if (!files.exists(path))
                {
                    continue;
                }

                auto sound = audioSystem.loadSound(files, path, description);

                if (!sound)
                {
                    std::clog << "Game: sound '" << path << "' not loaded: " << sound.error().message << '\n';
                    return std::nullopt;
                }

                return sound.value();
            }

            return std::nullopt;
        }

        uint32_t nextRandom(LightBuzz& buzz)
        {
            buzz.random ^= buzz.random << 13;
            buzz.random ^= buzz.random >> 17;
            buzz.random ^= buzz.random << 5;
            return buzz.random;
        }

        float randomRange(LightBuzz& buzz, float low, float high)
        {
            return low + (high - low) * static_cast<float>(nextRandom(buzz) % 10000) / 10000.0f;
        }

        std::vector<const BuzzSource*> nearestAudible(const LightBuzz& buzz, std::span<const BuzzSource> sources,
            const math::Vec3& listener)
        {
            std::vector<const BuzzSource*> audible;

            for (const BuzzSource& source : sources)
            {
                if (math::length(source.position - listener) < buzz.tuning.hearDistance)
                {
                    audible.push_back(&source);
                }
            }

            std::sort(audible.begin(), audible.end(),
                [&listener](const BuzzSource* left, const BuzzSource* right)
                {
                    return math::length(left->position - listener) < math::length(right->position - listener);
                });
            audible.resize(std::min<std::size_t>(audible.size(),
                static_cast<std::size_t>(std::clamp(buzz.tuning.maxVoices, 0, k_voiceLimit))));

            return audible;
        }

        void stopUnheard(LightBuzz& buzz, audio::AudioSystem& audioSystem, std::span<const BuzzSource* const> heard)
        {
            std::erase_if(buzz.voices,
                [&](const BuzzVoice& voice)
                {
                    const bool isHeard = std::any_of(heard.begin(), heard.end(),
                        [&voice](const BuzzSource* source)
                        {
                            return source->key == voice.key;
                        });
                    const bool isPlaying = audioSystem.isPlaying(voice.voice);

                    if (!isHeard && isPlaying)
                    {
                        audioSystem.stop(voice.voice);
                    }

                    return !isHeard || !isPlaying;
                });
        }

        void keepHumming(LightBuzz& buzz, audio::AudioSystem& audioSystem, const BuzzSource& source)
        {
            const float volume = buzz.tuning.buzzVolume * (source.level >= k_litLevel ? 1.0f : buzz.tuning.offBuzz);
            const auto playing = std::find_if(buzz.voices.begin(), buzz.voices.end(),
                [&source](const BuzzVoice& voice)
                {
                    return voice.key == source.key;
                });

            if (playing != buzz.voices.end())
            {
                audioSystem.setVoiceVolume(playing->voice, volume);
                return;
            }

            auto voice = audioSystem.play(*buzz.buzz, audio::PlayDescription{.bus = audio::Bus::World,
                .volume = volume,
                .pitch = randomRange(buzz, 0.98f, 1.02f),
                .position = source.position});

            if (voice)
            {
                buzz.voices.push_back({.key = source.key, .voice = voice.value()});
            }
        }

        void playCrackles(LightBuzz& buzz, audio::AudioSystem& audioSystem, std::span<const BuzzSource> sources,
            const math::Vec3& listener, double seconds)
        {
            buzz.lastCrackle.resize(sources.size(), -1.0e9);
            int played = 0;

            for (std::size_t index = 0; index < sources.size() && played < k_maxCracklesPerFrame; ++index)
            {
                const BuzzSource& source = sources[index];
                const bool isNear = math::length(source.position - listener) < buzz.tuning.hearDistance;
                const bool isRested = seconds - buzz.lastCrackle[index] >= buzz.tuning.crackleCooldown;

                if (!source.toggled || !isNear || !isRested)
                {
                    continue;
                }

                buzz.lastCrackle[index] = seconds;
                ++played;
                const audio::SoundHandle sound = buzz.crackles[nextRandom(buzz) % buzz.crackles.size()];
                static_cast<void>(audioSystem.play(sound, audio::PlayDescription{.bus = audio::Bus::World,
                    .volume = buzz.tuning.crackleVolume * randomRange(buzz, 0.6f, 1.0f),
                    .pitch = randomRange(buzz, 0.9f, 1.1f),
                    .position = source.position}));
            }
        }
    } //namespace

    void loadLightBuzz(LightBuzz& buzz, audio::AudioSystem& audioSystem, const lockstep::assets::FileSystem& files)
    {
        if (buzz.isLoaded)
        {
            return;
        }

        buzz.isLoaded = true;
        buzz.buzz = loadSound(audioSystem, files, std::string(k_buzzPath),
            audio::SoundDescription{.is3d = true, .isLooping = true, .minDistance = k_buzzMinDistance,
                .maxDistance = buzz.tuning.hearDistance});

        if (!buzz.buzz)
        {
            std::clog << "Game: sound '" << k_buzzPath << "' missing\n";
        }

        for (uint32_t number = 1; number <= k_maxCrackles; ++number)
        {
            const std::string stem = std::string(k_cracklePath) + std::to_string(number);
            std::optional<audio::SoundHandle> crackle = loadSound(audioSystem, files, stem,
                audio::SoundDescription{.is3d = true, .minDistance = k_crackleMinDistance,
                    .maxDistance = buzz.tuning.hearDistance});

            if (!crackle)
            {
                break;
            }

            buzz.crackles.push_back(*crackle);
        }

        if (buzz.crackles.empty())
        {
            std::clog << "Game: no sounds '" << k_cracklePath << "<n>'\n";
        }
    }

    void updateLightBuzz(LightBuzz& buzz, audio::AudioSystem& audioSystem, std::span<const BuzzSource> sources,
        const math::Vec3& listener, double seconds)
    {
        if (buzz.buzz)
        {
            const std::vector<const BuzzSource*> heard = nearestAudible(buzz, sources, listener);
            stopUnheard(buzz, audioSystem, heard);

            for (const BuzzSource* source : heard)
            {
                keepHumming(buzz, audioSystem, *source);
            }
        }

        if (!buzz.crackles.empty())
        {
            playCrackles(buzz, audioSystem, sources, listener, seconds);
        }
    }

    void stopLightBuzz(LightBuzz& buzz, audio::AudioSystem& audioSystem)
    {
        for (const BuzzVoice& voice : buzz.voices)
        {
            audioSystem.stop(voice.voice);
        }

        buzz.voices.clear();
        buzz.lastCrackle.clear();
    }

#if LOCKSTEP_DEV_TOOLS
    void lightBuzzDevSection(LightBuzz& buzz)
    {
        LightBuzzTuning& tuning = buzz.tuning;
        ImGui::Text("Buzz voices %zu, crackle sounds %zu%s", buzz.voices.size(), buzz.crackles.size(),
            buzz.buzz ? "" : " (buzz missing)");
        ImGui::SliderFloat("Buzz volume", &tuning.buzzVolume, 0.0f, 1.5f);
        ImGui::SliderFloat("Buzz while dark", &tuning.offBuzz, 0.0f, 1.0f);
        ImGui::SliderFloat("Crackle volume", &tuning.crackleVolume, 0.0f, 1.5f);
        ImGui::SliderFloat("Crackle cooldown (s)", &tuning.crackleCooldown, 0.0f, 1.0f);
        ImGui::SliderFloat("Hear distance (m)", &tuning.hearDistance, 2.0f, 30.0f);
        ImGui::SliderInt("Buzzing tubes", &tuning.maxVoices, 0, k_voiceLimit);
    }
#endif
} //namespace game
