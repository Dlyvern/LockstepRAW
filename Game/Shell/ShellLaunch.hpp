#ifndef LOCKSTEP_GAME_SHELL_SHELL_LAUNCH_HPP
#define LOCKSTEP_GAME_SHELL_SHELL_LAUNCH_HPP

#include <filesystem>
#include <span>
#include <string_view>
#include <vector>

namespace game
{
    // Decides whether the game starts in the main menu: yes with `--menu`, otherwise only when no
    // option asks for a game right away (`--level`, `--scene`, `--host`, `--connect`,
    // `--echo-play`, `--armed` or a `--dev-*` drill; the menu's own dev options `--dev-capture`,
    // `--dev-capture-on`, `--dev-gpu-timings`, `--dev-menu`, `--dev-auto-ready`,
    // `--dev-name` and `--dev-leave` don't count).
    // arguments: the command-line options, without the program name.
    // Returns:   true to start in the main menu.
    [[nodiscard]] bool isMenuLaunch(std::span<const char* const> arguments);

    // Returns: true if `--content` is among the options.
    // arguments: the command-line options, without the program name.
    [[nodiscard]] bool hasContentOption(std::span<const char* const> arguments);

    // The paks the menu mounts when no `--content` is given: the characters, the story levels and
    // the hospital, each only if it exists in `folder`.
    // folder:  the cooked content folder (Content/cooked of the source tree by default).
    // Returns: the existing paks, characters first.
    [[nodiscard]] std::vector<std::filesystem::path> defaultContentPaks(const std::filesystem::path& folder);
} //namespace game

#endif //LOCKSTEP_GAME_SHELL_SHELL_LAUNCH_HPP
