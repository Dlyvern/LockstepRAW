#ifndef LOCKSTEP_RENDER_GRAPHICS_CONFIG_HPP
#define LOCKSTEP_RENDER_GRAPHICS_CONFIG_HPP

#include "Lockstep/Core/Result.hpp"
#include "Lockstep/Render/GraphicsQuality.hpp"

#include <cstdint>
#include <filesystem>
#include <span>
#include <string>

namespace lockstep::render
{
    // The graphics options chosen at startup: quality and window size, from the command line and
    // the settings file.
    // quality:      the graphics quality (preset plus overrides).
    // windowWidth:  window width in pixels (960 for Low, 1280 otherwise, unless the file sets it).
    // windowHeight: window height in pixels (540 for Low, 720 otherwise, unless the file sets it).
    // file:         the settings file it was read from (or would be saved to).
    // origin:       where the preset came from, for the log ("--graphics", the file, "default").
    struct GraphicsConfig
    {
        GraphicsQuality quality{};
        uint32_t windowWidth{1280};
        uint32_t windowHeight{720};
        std::filesystem::path file;
        std::string origin;
    };

    // Where the settings file lives: `--settings <file>` if given, otherwise settings.ini in the
    // user config folder ($XDG_CONFIG_HOME or ~/.config on Linux, %APPDATA% on Windows, under
    // "LockstepRAW"), or next to the working directory if neither is known.
    // arguments: the command-line arguments, without the program name.
    // Returns:   the path (the file need not exist).
    [[nodiscard]] std::filesystem::path graphicsConfigPath(std::span<const char* const> arguments);

    // Reads the graphics options. The settings file (`key = value` lines, '#' comments; only lines
    // before the first `[section]` header and in `[graphics]` count, other sections belong to
    // other owners, see SettingsFile) sets `preset` and may override any quality field and `window_width` /
    // `window_height`; unknown keys and bad values are logged and skipped. `--graphics
    // <low|medium|high|ultra>` on the command line wins over the file: it takes that preset as is
    // and ignores the file's overrides (so test runs are predictable). With neither, High.
    // arguments: the command-line arguments, without the program name.
    // Returns:   the options, or an error if `--graphics` lacks a value or names no preset.
    [[nodiscard]] Result<GraphicsConfig> loadGraphicsConfig(std::span<const char* const> arguments);

    // Writes the quality into a settings file's `[graphics]` section: every quality field is
    // written, other lines and sections already in the file (window size, comments) are kept.
    // file:    the settings file; its folder is created if missing.
    // quality: the quality to store.
    // Returns: an error if the file could not be written.
    [[nodiscard]] Status saveGraphicsQuality(const std::filesystem::path& file, const GraphicsQuality& quality);

    // Writes the window size used at the next start into a settings file's `[graphics]` section,
    // keeping everything else in the file.
    // file:    the settings file; its folder is created if missing.
    // width:   window width in pixels (clamped to 320..7680).
    // height:  window height in pixels (clamped to 320..7680).
    // Returns: an error if the file could not be written.
    [[nodiscard]] Status saveWindowSize(const std::filesystem::path& file, uint32_t width, uint32_t height);
} //namespace lockstep::render

#endif //LOCKSTEP_RENDER_GRAPHICS_CONFIG_HPP
