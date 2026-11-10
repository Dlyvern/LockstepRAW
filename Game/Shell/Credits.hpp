#ifndef LOCKSTEP_GAME_SHELL_CREDITS_HPP
#define LOCKSTEP_GAME_SHELL_CREDITS_HPP

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace lockstep::assets
{
    class FileSystem;
} //namespace lockstep::assets

namespace game
{
    // How a credits line is shown.
    // Title:   a section heading.
    // Name:    an asset or person (the first column of a credits table).
    // Detail:  dim text under a name (author, licence, source) or a paragraph line.
    // Gap:     an empty line.
    enum class CreditKind : uint8_t
    {
        Title,
        Name,
        Detail,
        Gap
    };

    // One line of the credits screen.
    struct CreditLine
    {
        CreditKind kind{CreditKind::Detail};
        std::string text;
    };

    // Builds the credits screen from the project's CREDITS.md (sections, attribution tables and
    // notes; every CC BY entry is in it) and the sound credits `sounds/CREDITS.txt` from the
    // mounted content, after a fixed head (the game, the FMOD attribution).
    // creditsFile: CREDITS.md; a missing file is noted in the list.
    // files:       the mounted content.
    // Returns:     the lines, top to bottom.
    [[nodiscard]] std::vector<CreditLine> loadCredits(const std::filesystem::path& creditsFile,
        const lockstep::assets::FileSystem& files);
} //namespace game

#endif //LOCKSTEP_GAME_SHELL_CREDITS_HPP
