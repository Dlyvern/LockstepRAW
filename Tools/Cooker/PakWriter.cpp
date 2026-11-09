#include "PakWriter.hpp"

#include <Lockstep/Assets/FileSystem.hpp>
#include <Lockstep/Core/Hash.hpp>

#include "Assets/FileSystemMounts.hpp"
#include "Assets/OrderedWork.hpp"
#include "Assets/PakFormat.hpp"

#include <zstd.h>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <optional>
#include <string>
#include <system_error>
#include <vector>

namespace lockstep::cooker
{
    namespace
    {
        constexpr double k_bytesPerMegabyte{1024.0 * 1024.0};
        constexpr uint64_t k_minimumSavingDivisor{32};
        constexpr std::size_t k_filesPerWorker{4};

        struct PackedFile
        {
            std::vector<std::byte> stored;
            uint64_t bytes{0};
            uint64_t contentHash{0};
            assets::PakCompression compression{assets::PakCompression::None};
        };

        Result<std::vector<std::string>> collectPaths(const std::filesystem::path& directory)
        {
            std::error_code error;

            if (!std::filesystem::is_directory(directory, error))
            {
                return Error{ErrorCode::InvalidArgument, "pack: '" + directory.string() + "' is not a folder"};
            }

            std::vector<std::string> paths;

            for (std::filesystem::recursive_directory_iterator entry(directory, error), end;
                 !error && entry != end; entry.increment(error))
            {
                if (!entry->is_regular_file(error))
                {
                    continue;
                }

                auto normalized = assets::normalizePath(entry->path().lexically_relative(directory).generic_string());

                if (!normalized)
                {
                    return normalized.error();
                }

                paths.push_back(std::move(normalized).value());
            }

            if (error)
            {
                return Error{ErrorCode::AssetLoadFailed, "pack: cannot list '" + directory.string() + "'"};
            }

            std::sort(paths.begin(), paths.end());

            return paths;
        }

        Result<PackedFile> packFile(const std::filesystem::path& source, int level)
        {
            auto contents = assets::readOsFile(source);

            if (!contents)
            {
                return contents.error();
            }

            PackedFile packed{};
            packed.bytes = contents.value().size();
            packed.contentHash = hash64(contents.value());

            std::vector<std::byte> compressed(ZSTD_compressBound(contents.value().size()));
            const std::size_t size = ZSTD_compress(compressed.data(), compressed.size(), contents.value().data(),
                contents.value().size(), level);

            if (ZSTD_isError(size) == 0 && size < packed.bytes - packed.bytes / k_minimumSavingDivisor)
            {
                compressed.resize(size);
                compressed.shrink_to_fit();
                packed.stored = std::move(compressed);
                packed.compression = assets::PakCompression::Zstd;
                return packed;
            }

            packed.stored = std::move(contents).value();

            return packed;
        }

        uint64_t alignUp(uint64_t value)
        {
            return (value + assets::k_pakAlignment - 1) / assets::k_pakAlignment * assets::k_pakAlignment;
        }

        bool writeAt(std::ofstream& file, uint64_t offset, std::span<const std::byte> bytes)
        {
            file.seekp(static_cast<std::streamoff>(offset));
            return static_cast<bool>(file.write(reinterpret_cast<const char*>(bytes.data()),
                static_cast<std::streamsize>(bytes.size())));
        }

        std::vector<std::byte> buildToc(std::vector<assets::PakEntry>& entries, const std::vector<std::string>& paths,
            assets::PakHeader& header)
        {
            std::vector<std::size_t> order(entries.size());

            for (std::size_t index = 0; index < order.size(); ++index)
            {
                order[index] = index;
            }

            std::sort(order.begin(), order.end(), [&](std::size_t left, std::size_t right)
            {
                return entries[left].pathKey != entries[right].pathKey ? entries[left].pathKey < entries[right].pathKey
                    : paths[left] < paths[right];
            });

            std::vector<assets::PakEntry> sorted;
            std::string pathBytes;

            for (const std::size_t index : order)
            {
                assets::PakEntry entry = entries[index];
                entry.pathOffset = static_cast<uint32_t>(pathBytes.size());
                entry.pathBytes = static_cast<uint32_t>(paths[index].size());
                pathBytes += paths[index];
                sorted.push_back(entry);
            }

            entries = std::move(sorted);
            header.contentHash = assets::pakContentHash(entries, pathBytes);

            std::vector<std::byte> toc(entries.size() * sizeof(assets::PakEntry) + pathBytes.size());
            std::memcpy(toc.data(), entries.data(), entries.size() * sizeof(assets::PakEntry));
            std::memcpy(toc.data() + entries.size() * sizeof(assets::PakEntry), pathBytes.data(), pathBytes.size());
            header.tocBytes = toc.size();
            header.tocHash = hash64(toc);

            return toc;
        }
    } //namespace

    Status writePak(const PackOptions& options)
    {
        const auto start = std::chrono::steady_clock::now();
        auto paths = collectPaths(options.directory);

        if (!paths)
        {
            return paths.error();
        }

        std::ofstream file(options.pak, std::ios::binary | std::ios::trunc);

        if (!file)
        {
            return Error{ErrorCode::AssetLoadFailed, "pack: cannot write '" + options.pak.string() + "'"};
        }

        const std::vector<std::string>& files = paths.value();
        std::vector<assets::PakEntry> entries(files.size());
        std::optional<Error> failure;
        uint64_t offset = assets::k_pakAlignment;
        uint64_t rawBytes = 0;
        const uint32_t workers = assets::workerCountFor(options.threads, files.size());

        const auto pack = [&](std::size_t index)
        {
            return packFile(options.directory / std::filesystem::path(files[index]), options.zstdLevel);
        };

        const auto place = [&](std::size_t index, Result<PackedFile>&& packed)
        {
            if (failure)
            {
                return;
            }

            if (!packed)
            {
                failure = packed.error();
                return;
            }

            assets::PakEntry& entry = entries[index];
            entry.pathKey = assets::pathKey(files[index]);
            entry.offset = offset;
            entry.storedBytes = packed.value().stored.size();
            entry.bytes = packed.value().bytes;
            entry.contentHash = packed.value().contentHash;
            entry.compression = static_cast<uint32_t>(packed.value().compression);
            rawBytes += entry.bytes;

            if (!writeAt(file, offset, packed.value().stored))
            {
                failure = Error{ErrorCode::AssetLoadFailed, "pack: cannot write '" + options.pak.string() + "'"};
            }

            offset = alignUp(offset + entry.storedBytes);
        };

        assets::runOrdered<Result<PackedFile>>(files.size(), workers, workers * k_filesPerWorker, pack, place);

        if (failure)
        {
            return *failure;
        }

        assets::PakHeader header{};
        header.fileCount = static_cast<uint32_t>(entries.size());
        header.tocOffset = offset;
        const std::vector<std::byte> toc = buildToc(entries, files, header);

        if (!writeAt(file, header.tocOffset, toc)
            || !writeAt(file, 0, std::as_bytes(std::span(&header, 1))))
        {
            return Error{ErrorCode::AssetLoadFailed, "pack: cannot write '" + options.pak.string() + "'"};
        }

        const std::vector<std::byte> padding(assets::k_pakAlignment - sizeof(header));

        if (!writeAt(file, sizeof(header), padding))
        {
            return Error{ErrorCode::AssetLoadFailed, "pack: cannot write '" + options.pak.string() + "'"};
        }

        const uint64_t pakBytes = header.tocOffset + header.tocBytes;
        char hash[17]{};
        std::snprintf(hash, sizeof(hash), "%016llx", static_cast<unsigned long long>(header.contentHash));
        std::cout << std::fixed << std::setprecision(2) << "LockstepCooker: packed " << entries.size()
            << " files from '" << options.directory.string() << "' into '" << options.pak.string() << "': "
            << static_cast<double>(rawBytes) / k_bytesPerMegabyte << " MiB -> "
            << static_cast<double>(pakBytes) / k_bytesPerMegabyte << " MiB (zstd " << options.zstdLevel << ") in "
            << std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count()
            << " s, content hash " << hash << '\n' << std::defaultfloat;

        return {};
    }
} //namespace lockstep::cooker
