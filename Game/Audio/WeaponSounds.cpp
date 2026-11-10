#include "Audio/WeaponSounds.hpp"

#include <Lockstep/Assets/FileSystem.hpp>
#include <Lockstep/Audio/AudioSystem.hpp>

#include <iostream>
#include <string>
#include <string_view>

namespace game
{
    namespace
    {
        namespace audio = lockstep::audio;

        constexpr std::string_view k_folder{"sounds/"};
        constexpr std::string_view k_extension{".ogg"};
        constexpr uint32_t k_maxVariations{8};
        constexpr float k_minTailWeight{0.05f};

        std::optional<audio::SoundHandle> loadOne(audio::AudioSystem& audioSystem,
            const lockstep::assets::FileSystem& files, const std::string& stem, const audio::SoundDescription& description)
        {
            const std::string path = std::string(k_folder) + stem + std::string(k_extension);

            if (!files.exists(path))
            {
                return std::nullopt;
            }

            auto sound = audioSystem.loadSound(files, path, description);

            if (!sound)
            {
                std::clog << "Game: sound '" << path << "' not loaded: " << sound.error().message << '\n';
                return std::nullopt;
            }

            return sound.value();
        }

        std::vector<audio::SoundHandle> loadMany(audio::AudioSystem& audioSystem,
            const lockstep::assets::FileSystem& files, const std::string& stem, const audio::SoundDescription& description)
        {
            std::vector<audio::SoundHandle> sounds;

            for (uint32_t number = 1; number <= k_maxVariations; ++number)
            {
                std::optional<audio::SoundHandle> sound = loadOne(audioSystem, files, stem + std::to_string(number),
                    description);

                if (!sound)
                {
                    break;
                }

                sounds.push_back(*sound);
            }

            return sounds;
        }

        uint32_t nextRandom(WeaponSounds& sounds)
        {
            sounds.random ^= sounds.random << 13;
            sounds.random ^= sounds.random >> 17;
            sounds.random ^= sounds.random << 5;
            return sounds.random;
        }

        float randomRange(WeaponSounds& sounds, float low, float high)
        {
            return low + (high - low) * static_cast<float>(nextRandom(sounds) % 10000) / 10000.0f;
        }
    } //namespace

    WeaponSoundBank loadWeaponSounds(audio::AudioSystem& audioSystem, const lockstep::assets::FileSystem& files)
    {
        const audio::SoundDescription hands{.is3d = true, .minDistance = 1.0f, .maxDistance = 25.0f};
        const audio::SoundDescription small{.is3d = true, .minDistance = 0.6f, .maxDistance = 18.0f};
        WeaponSoundBank bank{};
        bank.magazineOut = loadOne(audioSystem, files, "weapons/v45_mag_out", hands);
        bank.magazineIn = loadOne(audioSystem, files, "weapons/v45_mag_in", hands);
        bank.charge = loadOne(audioSystem, files, "weapons/v45_charge", hands);

        for (std::size_t surface = 0; surface < k_surfaceCount; ++surface)
        {
            const std::string name = surfaceName(static_cast<Surface>(surface));
            bank.casings[surface] = loadMany(audioSystem, files, "casings/" + name + "_", small);
            bank.magazines[surface] = loadMany(audioSystem, files, "magazines/" + name + "_", small);
        }

        bank.indoorTails = loadMany(audioSystem, files, "tails/indoor_",
            audio::SoundDescription{.is3d = true, .minDistance = 4.0f, .maxDistance = 80.0f});
        bank.outdoorTails = loadMany(audioSystem, files, "tails/outdoor_",
            audio::SoundDescription{.is3d = true, .minDistance = 20.0f, .maxDistance = 500.0f});

        const std::size_t concrete = static_cast<std::size_t>(Surface::Concrete);

        if (!bank.magazineOut || !bank.magazineIn || !bank.charge || bank.casings[concrete].empty()
            || bank.magazines[concrete].empty() || bank.indoorTails.empty() || bank.outdoorTails.empty())
        {
            std::clog << "Game: some weapon detail sounds are missing (reload parts, casings, magazines or tails)\n";
        }

        return bank;
    }

    void playWeaponSound(WeaponSounds& sounds, audio::AudioSystem& audioSystem,
        const std::vector<audio::SoundHandle>& list, const math::Vec3& position, float volume, audio::Bus bus)
    {
        if (list.empty() || volume <= 0.0f)
        {
            return;
        }

        const audio::SoundHandle sound = list[nextRandom(sounds) % list.size()];
        static_cast<void>(audioSystem.play(sound, audio::PlayDescription{.bus = bus,
            .volume = volume * randomRange(sounds, 0.85f, 1.0f),
            .pitch = randomRange(sounds, 0.93f, 1.07f),
            .position = position}));
    }

    void playGunshotTail(WeaponSounds& sounds, audio::AudioSystem& audioSystem, const math::Vec3& shot, float indoor)
    {
        if (sounds.tailsThisFrame >= sounds.tuning.maxTails)
        {
            return;
        }

        ++sounds.tailsThisFrame;

        if (indoor > k_minTailWeight)
        {
            playWeaponSound(sounds, audioSystem, sounds.bank.indoorTails, shot, sounds.tuning.indoorTail * indoor);
        }

        if (indoor < 1.0f - k_minTailWeight)
        {
            playWeaponSound(sounds, audioSystem, sounds.bank.outdoorTails, shot,
                sounds.tuning.outdoorTail * (1.0f - indoor));
        }
    }

    void beginWeaponSounds(WeaponSounds& sounds)
    {
        sounds.tailsThisFrame = 0;
    }
} //namespace game
