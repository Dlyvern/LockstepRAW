#ifndef LOCKSTEP_GAME_INTRO_INTRO_LAUNCH_HPP
#define LOCKSTEP_GAME_INTRO_INTRO_LAUNCH_HPP

#include <Lockstep/Core/Result.hpp>

#include <cstdint>
#include <filesystem>
#include <span>

namespace game
{
    // What the startup intro shows at this launch.
    // None:        nothing (dev launches, or skipped after the first launch).
    // WarningOnly: only the content warning card (the very first launch while the intro is skipped).
    // Full:        every card.
    enum class IntroPlan : uint8_t
    {
        None,
        WarningOnly,
        Full
    };

    // The intro's entries in the user settings file (see lockstep::SettingsFile):
    // `[accessibility] skip_intro = true|false` (the Options -> Accessibility toggle) and
    // `[intro] warning_seen = true` (written once the warning card has been shown).
    // skipIntro:   the player skips the intro.
    // warningSeen: the content warning was shown at an earlier launch.
    struct IntroPreferences
    {
        bool skipIntro{false};
        bool warningSeen{false};
    };

    // Reads the intro's entries from the settings file. Missing or malformed values read as false.
    // file:    the settings file (render::graphicsConfigPath()).
    // Returns: the preferences.
    [[nodiscard]] IntroPreferences loadIntroPreferences(const std::filesystem::path& file);

    // Stores the Options -> Accessibility "Skip intro" toggle, keeping the rest of the file.
    // file:    the settings file.
    // skip:    skip the intro from the next launch on.
    // Returns: an error if the file could not be written.
    [[nodiscard]] lockstep::Status saveSkipIntro(const std::filesystem::path& file, bool skip);

    // Remembers that the content warning has been shown, keeping the rest of the file.
    // file:    the settings file.
    // Returns: an error if the file could not be written.
    [[nodiscard]] lockstep::Status saveWarningSeen(const std::filesystem::path& file);

    // Decides what the intro shows: `--intro` forces every card; launches that skip the main menu
    // (see isMenuLaunch()) show none; `--no-intro` and the settings file's skip toggle skip it too,
    // except that the content warning still shows once at the very first launch.
    // arguments:   the command line, without the program name.
    // preferences: the settings file's intro entries.
    // Returns:     the plan.
    [[nodiscard]] IntroPlan planIntro(std::span<const char* const> arguments, const IntroPreferences& preferences);
} //namespace game

#endif //LOCKSTEP_GAME_INTRO_INTRO_LAUNCH_HPP
