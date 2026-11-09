#ifndef LOCKSTEP_ASSETS_FILE_SYSTEM_HPP
#define LOCKSTEP_ASSETS_FILE_SYSTEM_HPP

#include "Lockstep/Core/Result.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace lockstep::assets
{
    struct FileSystemState;

    // What a mount reads from.
    // Directory: a loose folder (dev builds mount the cooker's output folder).
    // Pak:       a .pak archive written by LockstepCooker.
    enum class MountKind : uint8_t
    {
        Directory = 0,
        Pak
    };

    // One mounted source, as reported by FileSystem::mounts().
    // source:      the folder or .pak file.
    // kind:        directory or pak.
    // fileCount:   files in the pak; 0 for a directory (not scanned).
    // contentHash: the pak's content hash (covers every path and every file's contents, so two
    //              paks with equal hashes hold the same content); 0 for a directory.
    struct MountInfo
    {
        std::filesystem::path source;
        MountKind kind{MountKind::Directory};
        uint32_t fileCount{0};
        uint64_t contentHash{0};
    };

    // Turns a virtual path into its canonical form: forward slashes, no leading or trailing
    // slash, no empty, "." or ".." parts ("a/./b/../c" -> "a/c"). Case is kept.
    // path:    a virtual path; backslashes count as slashes.
    // Returns: the canonical path, or InvalidArgument if it is empty or ".." leaves the root.
    [[nodiscard]] Result<std::string> normalizePath(std::string_view path);

    // path:    a virtual path.
    // Returns: the 64-bit key a pak's table of contents uses for it: hash64 of the normalized
    //          path. Precondition: `path` is already normalized.
    [[nodiscard]] uint64_t pathKey(std::string_view path) noexcept;

    // The virtual file system all Engine asset loading reads through. Files are named by
    // virtual paths ("levels/sponza/scene"), resolved against an ordered list of mounts: the
    // first mount that has a path wins, so earlier mounts override later ones (a patch pak or a
    // loose dev folder mounted first replaces files in base.pak). Reads are thread-safe; mounting
    // is not and must happen before reads start.
    class FileSystem
    {
    public:
        // Creates a file system with no mounts.
        FileSystem();

        // Closes every mount.
        ~FileSystem();

        // Takes over `other`'s mounts. `other` becomes empty.
        FileSystem(FileSystem&& other) noexcept;

        // Closes this file system's mounts, then takes over `other`'s.
        FileSystem& operator=(FileSystem&& other) noexcept;

        FileSystem(const FileSystem&) = delete;
        FileSystem& operator=(const FileSystem&) = delete;

        // Adds a loose folder after the existing mounts (lowest priority so far). Virtual path
        // "a/b" is the file `directory/a/b`.
        // directory: an existing folder.
        // Returns:   success, or AssetLoadFailed if it is not a folder.
        [[nodiscard]] Status mountDirectory(const std::filesystem::path& directory);

        // Opens a .pak and adds it after the existing mounts. Its header and table of contents
        // are read and checked now; file contents are read on demand.
        // pak:     the archive.
        // Returns: success, or AssetLoadFailed if the file can't be opened, isn't a pak, has an
        //          unsupported version or a damaged table of contents.
        [[nodiscard]] Status mountPak(const std::filesystem::path& pak);

        // Mounts a folder with mountDirectory() or a file with mountPak().
        // source:  a folder or a .pak file.
        // Returns: success, or the mount's error (AssetLoadFailed if `source` doesn't exist).
        [[nodiscard]] Status mount(const std::filesystem::path& source);

        // path:    a virtual path.
        // Returns: true if any mount has a file at the normalized path.
        [[nodiscard]] bool exists(std::string_view path) const;

        // Reads a whole file from the first mount that has it. Pak files are decompressed and
        // their content hash is checked.
        // path:    a virtual path.
        // Returns: the contents, or InvalidArgument for a bad path, AssetLoadFailed if no mount
        //          has it, it can't be read, or its contents don't match their hash.
        [[nodiscard]] Result<std::vector<std::byte>> read(std::string_view path) const;

        // Lists the files under a virtual folder, over all mounts.
        // directory: a virtual folder ("levels/sponza"); empty lists everything.
        // Returns:   the virtual paths of every file below it at any depth, sorted, each once.
        [[nodiscard]] std::vector<std::string> list(std::string_view directory) const;

        // Returns: the mounts in priority order (first wins). Pak content hashes let two peers
        //          check that they run the same content.
        [[nodiscard]] std::span<const MountInfo> mounts() const noexcept;

    private:
        std::unique_ptr<FileSystemState> m_state;
    };

    // Reads every `--content <folder|pak>` option. Mount order is the order on the command line,
    // so the first --content has the highest priority.
    // arguments: command-line options.
    // Returns:   the sources in order (empty without --content), or InvalidArgument for a
    //            --content with no value.
    [[nodiscard]] Result<std::vector<std::filesystem::path>> parseContentMounts(std::span<const char* const> arguments);
} //namespace lockstep::assets

#endif //LOCKSTEP_ASSETS_FILE_SYSTEM_HPP
