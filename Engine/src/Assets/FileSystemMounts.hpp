#ifndef LOCKSTEP_ASSETS_FILE_SYSTEM_MOUNTS_HPP
#define LOCKSTEP_ASSETS_FILE_SYSTEM_MOUNTS_HPP

#include "Lockstep/Assets/FileSystem.hpp"
#include "Lockstep/Core/Result.hpp"

#include <cstddef>
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace lockstep::assets
{
    // One source of files in a FileSystem. Paths passed in are already normalized. Every
    // function may be called from several threads at once.
    class Mount
    {
    public:
        virtual ~Mount() = default;

        // path:    a normalized virtual path.
        // Returns: true if this mount has a file there.
        [[nodiscard]] virtual bool exists(const std::string& path) const = 0;

        // Reads a file this mount has (check exists() first).
        // path:    a normalized virtual path.
        // Returns: the contents, or AssetLoadFailed.
        [[nodiscard]] virtual Result<std::vector<std::byte>> read(const std::string& path) const = 0;

        // Appends the paths of every file below a folder.
        // directory: a normalized virtual folder, or empty for everything.
        // paths:     receives the paths, unsorted.
        virtual void list(std::string_view directory, std::vector<std::string>& paths) const = 0;
    };

    // Everything behind a FileSystem: the mounts and their descriptions, in priority order.
    struct FileSystemState
    {
        std::vector<std::unique_ptr<Mount>> mounts;
        std::vector<MountInfo> infos;
    };

    // directory: an existing folder.
    // Returns:   a mount reading loose files below it.
    [[nodiscard]] std::unique_ptr<Mount> createDirectoryMount(const std::filesystem::path& directory);

    // Opens a pak and reads its table of contents.
    // pak:     the file.
    // info:    receives its file count and content hash.
    // Returns: the mount, or AssetLoadFailed for a missing, foreign, newer or damaged pak.
    [[nodiscard]] Result<std::unique_ptr<Mount>> openPakMount(const std::filesystem::path& pak, MountInfo& info);

    // Reads a whole OS file.
    // path:    the file.
    // Returns: its contents, or AssetLoadFailed.
    [[nodiscard]] Result<std::vector<std::byte>> readOsFile(const std::filesystem::path& path);
} //namespace lockstep::assets

#endif //LOCKSTEP_ASSETS_FILE_SYSTEM_MOUNTS_HPP
