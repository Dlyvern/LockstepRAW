#ifndef LOCKSTEP_CORE_SETTINGS_FILE_HPP
#define LOCKSTEP_CORE_SETTINGS_FILE_HPP

#include "Lockstep/Core/Result.hpp"

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace lockstep
{
    // A user settings file of `key = value` lines grouped under `[section]` headers ('#' starts a
    // comment; lines before the first header belong to the unnamed section ""). Changing values
    // keeps every other line, comment and the order of the file, so several owners (graphics,
    // audio, controls) can share one file.
    class SettingsFile
    {
    public:
        // Reads a settings file. A missing or unreadable file gives an empty one that save()
        // creates.
        // file:    the file.
        // Returns: its contents.
        [[nodiscard]] static SettingsFile load(const std::filesystem::path& file);

        // Looks a value up.
        // section: the section name without brackets ("" for lines before the first header).
        // key:     the key.
        // Returns: the trimmed value of the last matching line, or nothing.
        [[nodiscard]] std::optional<std::string_view> value(std::string_view section, std::string_view key) const;

        // Sets a value: replaces the matching line, or adds the line at the end of its section,
        // or adds the section at the end of the file.
        // section: the section name without brackets.
        // key:     the key.
        // value:   the value, written as is.
        void set(std::string_view section, std::string_view key, std::string_view value);

        // Returns: true if some line of `section` has `key`.
        // section: the section name without brackets.
        // key:     the key.
        [[nodiscard]] bool contains(std::string_view section, std::string_view key) const;

        // Writes the file, creating its folder if missing.
        // Returns: an error if the file could not be written.
        [[nodiscard]] Status save() const;

        // Returns: the file this was loaded from and saves to.
        [[nodiscard]] const std::filesystem::path& path() const noexcept;

    private:
        // One line of the file, with the section it is in and, for `key = value` lines, its key.
        struct Line
        {
            std::string text;
            std::string section;
            std::string key;
            std::string value;
            bool isHeader{false};
        };

        std::filesystem::path m_file;
        std::vector<Line> m_lines;
    };
} //namespace lockstep

#endif //LOCKSTEP_CORE_SETTINGS_FILE_HPP
