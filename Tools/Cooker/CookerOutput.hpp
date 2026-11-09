#ifndef LOCKSTEP_COOKER_COOKER_OUTPUT_HPP
#define LOCKSTEP_COOKER_COOKER_OUTPUT_HPP

#include <Lockstep/Core/Result.hpp>

#include <cstddef>
#include <filesystem>
#include <span>
#include <string_view>

namespace lockstep::cooker
{
    // Writes a file below the output folder, creating folders as needed. Thread-safe for
    // different paths.
    // root:    the output folder (it maps to the virtual root).
    // path:    the file's virtual path.
    // bytes:   the contents.
    // Returns: success, or AssetLoadFailed if the file can't be written.
    [[nodiscard]] Status writeCookedFile(const std::filesystem::path& root, std::string_view path,
        std::span<const std::byte> bytes);
} //namespace lockstep::cooker

#endif //LOCKSTEP_COOKER_COOKER_OUTPUT_HPP
