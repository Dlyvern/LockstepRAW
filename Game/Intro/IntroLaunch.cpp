#include "Intro/IntroLaunch.hpp"

#include "Shell/ShellLaunch.hpp"

#include <Lockstep/Core/SettingsFile.hpp>

#include <algorithm>
#include <optional>
#include <string_view>

namespace game
{
    namespace
    {
        constexpr std::string_view k_accessibilitySection{"accessibility"};
        constexpr std::string_view k_skipKey{"skip_intro"};
        constexpr std::string_view k_introSection{"intro"};
        constexpr std::string_view k_warningSeenKey{"warning_seen"};
        constexpr std::string_view k_forceFlag{"--intro"};
        constexpr std::string_view k_noIntroFlag{"--no-intro"};

        bool readBool(const lockstep::SettingsFile& settings, std::string_view section, std::string_view key)
        {
            const std::optional<std::string_view> value = settings.value(section, key);
            return value && (*value == "true" || *value == "1" || *value == "yes" || *value == "on");
        }

        bool hasFlag(std::span<const char* const> arguments, std::string_view flag)
        {
            return std::any_of(arguments.begin(), arguments.end(), [flag](const char* argument)
            {
                return argument != nullptr && std::string_view(argument) == flag;
            });
        }
    } //namespace

    IntroPreferences loadIntroPreferences(const std::filesystem::path& file)
    {
        const lockstep::SettingsFile settings = lockstep::SettingsFile::load(file);
        return IntroPreferences{.skipIntro = readBool(settings, k_accessibilitySection, k_skipKey),
            .warningSeen = readBool(settings, k_introSection, k_warningSeenKey)};
    }

    lockstep::Status saveSkipIntro(const std::filesystem::path& file, bool skip)
    {
        lockstep::SettingsFile settings = lockstep::SettingsFile::load(file);
        settings.set(k_accessibilitySection, k_skipKey, skip ? "true" : "false");
        return settings.save();
    }

    lockstep::Status saveWarningSeen(const std::filesystem::path& file)
    {
        lockstep::SettingsFile settings = lockstep::SettingsFile::load(file);
        settings.set(k_introSection, k_warningSeenKey, "true");
        return settings.save();
    }

    IntroPlan planIntro(std::span<const char* const> arguments, const IntroPreferences& preferences)
    {
        if (hasFlag(arguments, k_forceFlag))
        {
            return IntroPlan::Full;
        }

        if (!isMenuLaunch(arguments))
        {
            return IntroPlan::None;
        }

        if (hasFlag(arguments, k_noIntroFlag) || preferences.skipIntro)
        {
            return preferences.warningSeen ? IntroPlan::None : IntroPlan::WarningOnly;
        }

        return IntroPlan::Full;
    }
} //namespace game
