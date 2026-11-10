#include "Shell/UserSettings.hpp"

#include "GameState.hpp"

#include <Lockstep/Audio/AudioSystem.hpp>
#include <Lockstep/Core/SettingsFile.hpp>
#include <Lockstep/Platform/Window.hpp>
#include <Lockstep/Render/Renderer.hpp>

#include <algorithm>
#include <array>
#include <charconv>
#include <sstream>

namespace game
{
    namespace
    {
        constexpr std::string_view k_audio{"audio"};
        constexpr std::string_view k_controls{"controls"};
        constexpr std::string_view k_accessibility{"accessibility"};
        constexpr std::string_view k_display{"display"};
        constexpr std::string_view k_player{"player"};
        constexpr std::string_view k_graphics{"graphics"};
        constexpr std::string_view k_secondarySuffix{"_alt"};
        constexpr float k_defaultFieldOfView{60.0f};
        constexpr float k_reducedMotion{0.33f};
        constexpr std::size_t k_maxNameLength{20};
        constexpr std::string_view k_presentFlag{"--present"};

        std::optional<float> readFloat(const lockstep::SettingsFile& file, std::string_view section, std::string_view key)
        {
            const std::optional<std::string_view> text = file.value(section, key);

            if (!text)
            {
                return std::nullopt;
            }

            float value = 0.0f;
            const auto [end, error] = std::from_chars(text->data(), text->data() + text->size(), value);

            if (error != std::errc{} || end != text->data() + text->size())
            {
                return std::nullopt;
            }

            return value;
        }

        std::optional<bool> readBool(const lockstep::SettingsFile& file, std::string_view section, std::string_view key)
        {
            const std::optional<std::string_view> text = file.value(section, key);

            if (text == "true" || text == "1" || text == "on")
            {
                return true;
            }

            if (text == "false" || text == "0" || text == "off")
            {
                return false;
            }

            return std::nullopt;
        }

        void readVolume(const lockstep::SettingsFile& file, std::string_view key, float& volume)
        {
            volume = std::clamp(readFloat(file, k_audio, key).value_or(volume), 0.0f, 1.0f);
        }

        std::string formatFloat(float value)
        {
            std::ostringstream stream;
            stream << value;
            return stream.str();
        }

        std::string secondaryKey(Action action)
        {
            return std::string(actionSettingsKey(action)) + std::string(k_secondarySuffix);
        }

        void readBindings(const lockstep::SettingsFile& file, KeyBindings& bindings)
        {
            for (std::size_t index = 0; index < k_actionCount; ++index)
            {
                const auto action = static_cast<Action>(index);
                ActionBinding& binding = bindings.actions[index];

                if (const std::optional<std::string_view> text = file.value(k_controls, actionSettingsKey(action)))
                {
                    binding.primary = parseBinding(*text).value_or(binding.primary);
                }

                if (const std::optional<std::string_view> text = file.value(k_controls, secondaryKey(action)))
                {
                    binding.secondary = parseBinding(*text).value_or(binding.secondary);
                }
            }
        }
    } //namespace

    UserSettings loadUserSettings(const std::filesystem::path& file)
    {
        const lockstep::SettingsFile settingsFile = lockstep::SettingsFile::load(file);
        UserSettings settings{};
        readVolume(settingsFile, "master", settings.masterVolume);
        readVolume(settingsFile, "world", settings.worldVolume);
        readVolume(settingsFile, "player", settings.playerVolume);
        readVolume(settingsFile, "ambience", settings.ambienceVolume);
        readVolume(settingsFile, "music", settings.musicVolume);
        settings.mouseSensitivity = std::clamp(readFloat(settingsFile, k_controls, "mouse_sensitivity")
            .value_or(settings.mouseSensitivity), 0.0002f, 0.02f);
        settings.invertY = readBool(settingsFile, k_controls, "invert_y").value_or(settings.invertY);
        settings.fieldOfView = std::clamp(readFloat(settingsFile, k_controls, "field_of_view")
            .value_or(settings.fieldOfView), k_minFieldOfView, k_maxFieldOfView);
        readBindings(settingsFile, settings.bindings);
        settings.subtitles = readBool(settingsFile, k_accessibility, "subtitles").value_or(settings.subtitles);
        settings.reduceHeadBob = readBool(settingsFile, k_accessibility, "reduce_head_bob").value_or(settings.reduceHeadBob);
        settings.reduceMotionBlur = readBool(settingsFile, k_accessibility, "reduce_motion_blur")
            .value_or(settings.reduceMotionBlur);
        settings.skipIntro = readBool(settingsFile, k_accessibility, "skip_intro").value_or(settings.skipIntro);
        if (const std::optional<std::string_view> present = settingsFile.value(k_display, "present_mode"))
        {
            const std::string flag(k_presentFlag);
            const std::string value(*present);
            const std::array<const char*, 2> arguments{flag.c_str(), value.c_str()};

            if (auto parsed = lockstep::render::parsePresentMode(arguments))
            {
                settings.presentMode = parsed.value();
            }
        }

        settings.fullscreen = readBool(settingsFile, k_display, "fullscreen").value_or(settings.fullscreen);
        settings.windowWidth = static_cast<uint32_t>(readFloat(settingsFile, k_graphics, "window_width")
            .value_or(static_cast<float>(settings.windowWidth)));
        settings.windowHeight = static_cast<uint32_t>(readFloat(settingsFile, k_graphics, "window_height")
            .value_or(static_cast<float>(settings.windowHeight)));

        if (const std::optional<std::string_view> name = settingsFile.value(k_player, "name"); name && !name->empty())
        {
            settings.playerName = std::string(name->substr(0, k_maxNameLength));
        }

        if (const std::optional<std::string_view> address = settingsFile.value(k_player, "address"))
        {
            settings.address = std::string(*address);
        }

        const float port = readFloat(settingsFile, k_player, "port").value_or(static_cast<float>(settings.port));
        settings.port = static_cast<uint16_t>(std::clamp(port, 1.0f, 65535.0f));

        return settings;
    }

    lockstep::Status saveUserSettings(const std::filesystem::path& file, const UserSettings& settings)
    {
        lockstep::SettingsFile settingsFile = lockstep::SettingsFile::load(file);
        settingsFile.set(k_audio, "master", formatFloat(settings.masterVolume));
        settingsFile.set(k_audio, "world", formatFloat(settings.worldVolume));
        settingsFile.set(k_audio, "player", formatFloat(settings.playerVolume));
        settingsFile.set(k_audio, "ambience", formatFloat(settings.ambienceVolume));
        settingsFile.set(k_audio, "music", formatFloat(settings.musicVolume));
        settingsFile.set(k_controls, "mouse_sensitivity", formatFloat(settings.mouseSensitivity));
        settingsFile.set(k_controls, "invert_y", settings.invertY ? "true" : "false");
        settingsFile.set(k_controls, "field_of_view", formatFloat(settings.fieldOfView));

        for (std::size_t index = 0; index < k_actionCount; ++index)
        {
            const auto action = static_cast<Action>(index);
            settingsFile.set(k_controls, actionSettingsKey(action), bindingName(settings.bindings.actions[index].primary));
            settingsFile.set(k_controls, secondaryKey(action), bindingName(settings.bindings.actions[index].secondary));
        }

        settingsFile.set(k_accessibility, "subtitles", settings.subtitles ? "true" : "false");
        settingsFile.set(k_accessibility, "reduce_head_bob", settings.reduceHeadBob ? "true" : "false");
        settingsFile.set(k_accessibility, "reduce_motion_blur", settings.reduceMotionBlur ? "true" : "false");
        settingsFile.set(k_accessibility, "skip_intro", settings.skipIntro ? "true" : "false");
        settingsFile.set(k_display, "present_mode", lockstep::render::toString(settings.presentMode));
        settingsFile.set(k_display, "fullscreen", settings.fullscreen ? "true" : "false");
        settingsFile.set(k_player, "name", settings.playerName);
        settingsFile.set(k_player, "address", settings.address);
        settingsFile.set(k_player, "port", std::to_string(settings.port));

        return settingsFile.save();
    }

    void applyUserSettings(const UserSettings& settings, lockstep::app::GameState& state,
        lockstep::app::EngineContext& engine)
    {
        namespace audio = lockstep::audio;
        engine.audio.setBusVolume(audio::Bus::Master, settings.masterVolume);
        engine.audio.setBusVolume(audio::Bus::World, settings.worldVolume);
        engine.audio.setBusVolume(audio::Bus::Player, settings.playerVolume);
        engine.audio.setBusVolume(audio::Bus::Ambience, settings.ambienceVolume);
        engine.audio.setBusVolume(audio::Bus::Music, settings.musicVolume);

        ControlSettings& controls = state.settings.controls;
        controls.mouseSensitivity = settings.mouseSensitivity;
        controls.invertY = settings.invertY;
        controls.bindings = settings.bindings;

        const HeadMotionTuning defaults{};
        const float motion = settings.reduceHeadBob ? k_reducedMotion : 1.0f;
        HeadMotionTuning& tuning = state.headMotion.tuning;
        tuning.walkBob = defaults.walkBob * motion;
        tuning.runBob = defaults.runBob * motion;
        tuning.crouchBob = defaults.crouchBob * motion;
        tuning.strafeRoll = defaults.strafeRoll * motion;
        tuning.slideRoll = defaults.slideRoll * motion;
        tuning.vaultRoll = defaults.vaultRoll * motion;
        tuning.landMaxDip = defaults.landMaxDip * motion;
        tuning.fieldOfViewOffset = math::radians(settings.fieldOfView - k_defaultFieldOfView);
        state.hud.showSubtitles = settings.subtitles;

        if (engine.window.isFullscreen() != settings.fullscreen)
        {
            engine.window.setFullscreen(settings.fullscreen);
        }
    }

    void enforceUserLook(const UserSettings& settings, lockstep::app::EngineContext& engine)
    {
        if (settings.reduceMotionBlur)
        {
            engine.renderer.settings().motionBlur.enabled = false;
        }
    }
} //namespace game
