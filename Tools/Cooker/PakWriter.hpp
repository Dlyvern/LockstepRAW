#ifndef LOCKSTEP_COOKER_PAK_WRITER_HPP
#define LOCKSTEP_COOKER_PAK_WRITER_HPP

#include <Lockstep/Core/Result.hpp>

#include <cstdint>
#include <filesystem>

namespace lockstep::cooker
{
    // What to pack.
    // directory:  a cooked folder; every file below it is packed under its relative path.
    // pak:        the .pak to write (replaced if it exists).
    // zstdLevel:  zstd compression level (1-22). A file is stored raw when compression saves
    //             less than a few percent.
    // threads:    compression threads; 0 picks the hardware thread count.
    struct PackOptions
    {
        std::filesystem::path directory;
        std::filesystem::path pak;
        int zstdLevel{19};
        uint32_t threads{0};
    };

    // Writes a pak (see Engine/src/Assets/PakFormat.hpp): files in path order, each 4 KB
    // aligned and compressed with zstd when that helps, then the table of contents. The same
    // folder contents give the same bytes. Prints the file count, sizes and content hash.
    // options: source folder, target file and compression.
    // Returns: success, or an error if a file can't be read, a path is invalid, or the pak
    //          can't be written.
    [[nodiscard]] Status writePak(const PackOptions& options);
} //namespace lockstep::cooker

#endif //LOCKSTEP_COOKER_PAK_WRITER_HPP
