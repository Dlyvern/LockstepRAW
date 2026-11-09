#ifndef LOCKSTEP_ASSETS_PAK_FORMAT_HPP
#define LOCKSTEP_ASSETS_PAK_FORMAT_HPP

#include <array>
#include <cstdint>
#include <span>
#include <string_view>

// The .pak layout, shared by the Engine's reader and LockstepCooker's writer. All integers are
// little-endian.
//
//   PakHeader                      at offset 0
//   file data                      each file starts at a multiple of k_pakAlignment
//   PakEntry[fileCount]            the table of contents, at PakHeader::tocOffset, sorted by
//                                  (pathKey, path)
//   path bytes                     every entry's path, concatenated without terminators
//
// A file is stored raw or as one zstd frame. Version changes whenever the layout does; readers
// reject versions they don't know.
namespace lockstep::assets
{
    inline constexpr std::array<char, 4> k_pakMagic{'L', 'P', 'A', 'K'};
    inline constexpr uint32_t k_pakVersion{1};
    inline constexpr uint64_t k_pakAlignment{4096};

    // How one file's bytes are stored.
    enum class PakCompression : uint32_t
    {
        None = 0,
        Zstd = 1
    };

    // The fixed header at the start of a pak.
    // magic:       k_pakMagic.
    // version:     k_pakVersion of the writer.
    // fileCount:   PakEntry count.
    // flags:       reserved, 0.
    // tocOffset:   where the entries start.
    // tocBytes:    size of the entries plus the path bytes.
    // tocHash:     hash64 of those tocBytes, to detect a damaged table.
    // contentHash: pakContentHash() of the entries.
    struct PakHeader
    {
        std::array<char, 4> magic{k_pakMagic};
        uint32_t version{k_pakVersion};
        uint32_t fileCount{0};
        uint32_t flags{0};
        uint64_t tocOffset{0};
        uint64_t tocBytes{0};
        uint64_t tocHash{0};
        uint64_t contentHash{0};
    };

    // One file in the table of contents.
    // pathKey:     pathKey() of its virtual path.
    // offset:      where its stored bytes start (a multiple of k_pakAlignment).
    // storedBytes: bytes in the pak.
    // bytes:       bytes after decompression.
    // contentHash: hash64 of the decompressed bytes.
    // compression: a PakCompression value.
    // pathOffset:  where its path starts in the path bytes.
    // pathBytes:   length of its path.
    // reserved:    0.
    struct PakEntry
    {
        uint64_t pathKey{0};
        uint64_t offset{0};
        uint64_t storedBytes{0};
        uint64_t bytes{0};
        uint64_t contentHash{0};
        uint32_t compression{0};
        uint32_t pathOffset{0};
        uint32_t pathBytes{0};
        uint32_t reserved{0};
    };

    static_assert(sizeof(PakHeader) == 48);
    static_assert(sizeof(PakEntry) == 56);

    // The content hash of a pak: hash64 over each entry's path and file content hash, in table
    // order. Independent of compression and layout, so equal hashes mean equal content.
    // entries: the table of contents, sorted.
    // paths:   the path bytes.
    // Returns: the hash.
    [[nodiscard]] uint64_t pakContentHash(std::span<const PakEntry> entries, std::string_view paths) noexcept;
} //namespace lockstep::assets

#endif //LOCKSTEP_ASSETS_PAK_FORMAT_HPP
