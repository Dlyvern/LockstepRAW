#ifndef LOCKSTEP_COOKER_COOKER_ARGUMENTS_HPP
#define LOCKSTEP_COOKER_COOKER_ARGUMENTS_HPP

#include <Lockstep/Core/Result.hpp>

#include "PakWriter.hpp"
#include "SceneCook.hpp"

#include <span>
#include <variant>

namespace lockstep::cooker
{
    // What the command line asked for: cook a scene or pack a folder.
    using CookerCommand = std::variant<CookOptions, PackOptions>;

    // Usage text printed for bad arguments.
    inline constexpr const char* k_usage =
        "usage:\n"
        "  LockstepCooker <source.gltf> --out <dir> --name <virtual prefix> [--scale f] [--threads n]\n"
        "      [--max-texture-size n] [--texels-per-metre n] [--no-auto-cap] [--limbs [--wound <wound.gltf>]]\n"
        "      [--height-dir <dir>]... [--no-surface-detail] [--single-sided]\n"
        "  LockstepCooker pack <cooked dir> <out.pak> [--zstd-level n] [--threads n]\n";

    // Parses the cooker's command line.
    // arguments: the options without the program name.
    // Returns:   the command, or InvalidArgument naming what is missing or malformed.
    [[nodiscard]] Result<CookerCommand> parseCookerArguments(std::span<const char* const> arguments);
} //namespace lockstep::cooker

#endif //LOCKSTEP_COOKER_COOKER_ARGUMENTS_HPP
