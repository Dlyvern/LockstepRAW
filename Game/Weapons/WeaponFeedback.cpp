#include "Weapons/WeaponFeedback.hpp"

#include <Lockstep/Assets/FileSystem.hpp>
#include <Lockstep/Audio/AudioSystem.hpp>

#if LOCKSTEP_DEV_TOOLS
#include <imgui.h>
#endif

#include <iostream>
#include <string>
#include <string_view>

namespace game
{
    namespace
    {
        namespace audio = lockstep::audio;

        std::optional<audio::SoundHandle> loadOne(audio::AudioSystem& audioSystem,
            const lockstep::assets::FileSystem& files, std::string_view stem, const audio::SoundDescription& description)
        {
            const std::string path = "sounds/" + std::string(stem) + ".ogg";

            if (!files.exists(path))
            {
                std::clog << "Game: sound '" << path << "' missing\n";
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

        void play(audio::AudioSystem& audioSystem, const std::optional<audio::SoundHandle>& sound, float volume,
            const std::optional<math::Vec3>& position)
        {
            if (!sound)
            {
                return;
            }

            static_cast<void>(audioSystem.play(*sound, audio::PlayDescription{.bus = audio::Bus::Player,
                .volume = volume,
                .position = position}));
        }
    } //namespace

    HandlingSounds loadHandlingSounds(audio::AudioSystem& audioSystem, const lockstep::assets::FileSystem& files)
    {
        const audio::SoundDescription near{.is3d = true, .minDistance = 0.5f, .maxDistance = 15.0f};
        const audio::SoundDescription own{.is3d = false};

        return HandlingSounds{.flashlightClick = loadOne(audioSystem, files, "weapons/flashlight_click", near),
            .fireModeSwitch = loadOne(audioSystem, files, "weapons/firemode_switch", own),
            .breathHold = loadOne(audioSystem, files, "player/breath_hold", own),
            .breathRelease = loadOne(audioSystem, files, "player/breath_release", own)};
    }

    void presentWeaponFeedback(WeaponFeedback& feedback, audio::AudioSystem& audioSystem,
        const WeaponFeedbackFrame& frame)
    {
        if (!feedback.hasLast)
        {
            feedback.lastFlashlight = frame.flashlightOn;
            feedback.lastMode = frame.mode;
            feedback.lastHolds = frame.holds;
            feedback.lastReleases = frame.releases;
            feedback.hasLast = true;
        }

        const WeaponFeedbackTuning& tuning = feedback.tuning;

        for (std::size_t index = 0; index < k_playerCount; ++index)
        {
            if (frame.flashlightOn[index] != feedback.lastFlashlight[index] && frame.positions[index])
            {
                play(audioSystem, feedback.sounds.flashlightClick, tuning.clickVolume, frame.positions[index]);
            }

            feedback.lastFlashlight[index] = frame.flashlightOn[index];
        }

        if (frame.mode != feedback.lastMode)
        {
            play(audioSystem, feedback.sounds.fireModeSwitch, tuning.switchVolume, std::nullopt);
            feedback.lastMode = frame.mode;
        }

        if (frame.holds != feedback.lastHolds)
        {
            play(audioSystem, feedback.sounds.breathHold, tuning.breathVolume, std::nullopt);
            feedback.lastHolds = frame.holds;
        }

        if (frame.releases != feedback.lastReleases)
        {
            play(audioSystem, feedback.sounds.breathRelease, tuning.breathVolume, std::nullopt);
            feedback.lastReleases = frame.releases;
        }
    }

#if LOCKSTEP_DEV_TOOLS
    void weaponFeedbackDevSection(WeaponFeedback& feedback)
    {
        if (!ImGui::TreeNode("Weapon sounds and cues"))
        {
            return;
        }

        WeaponFeedbackTuning& tuning = feedback.tuning;
        ImGui::SliderFloat("Flashlight click", &tuning.clickVolume, 0.0f, 2.0f);
        ImGui::SliderFloat("Fire mode switch", &tuning.switchVolume, 0.0f, 2.0f);
        ImGui::SliderFloat("Breath", &tuning.breathVolume, 0.0f, 2.0f);
        ImGui::SliderFloat("Held breath focus", &tuning.holdFocus, 0.0f, 1.0f);
        ImGui::TreePop();
    }
#endif
} //namespace game
