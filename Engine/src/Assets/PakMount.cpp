#include "FileSystemMounts.hpp"

#include "Lockstep/Core/Hash.hpp"

#include "PakFormat.hpp"

#include <zstd.h>

#include <algorithm>
#include <cstring>
#include <fstream>
#include <mutex>
#include <string_view>

namespace lockstep::assets
{
    namespace
    {
        Error pakError(const std::filesystem::path& pak, std::string_view what)
        {
            return Error{ErrorCode::AssetLoadFailed, "pak '" + pak.string() + "': " + std::string(what)};
        }

        bool entryBefore(const PakEntry& entry, uint64_t key)
        {
            return entry.pathKey < key;
        }

        class PakMount final : public Mount
        {
        public:
            PakMount(std::filesystem::path source, std::ifstream file, std::vector<PakEntry> entries,
                std::string paths)
                : m_source(std::move(source)),
                  m_file(std::move(file)),
                  m_entries(std::move(entries)),
                  m_paths(std::move(paths))
            {
            }

            bool exists(const std::string& path) const override
            {
                return find(path) != nullptr;
            }

            Result<std::vector<std::byte>> read(const std::string& path) const override
            {
                const PakEntry* entry = find(path);

                if (entry == nullptr)
                {
                    return pakError(m_source, "no file '" + path + "'");
                }

                auto stored = readStored(*entry);

                if (!stored)
                {
                    return stored.error();
                }

                auto contents = unpack(*entry, std::move(stored).value());

                if (!contents)
                {
                    return contents.error();
                }

                if (hash64(contents.value()) != entry->contentHash)
                {
                    return pakError(m_source, "'" + path + "' does not match its content hash");
                }

                return contents;
            }

            void list(std::string_view directory, std::vector<std::string>& paths) const override
            {
                for (const PakEntry& entry : m_entries)
                {
                    const std::string_view path = pathOf(entry);

                    if (directory.empty() || path.starts_with(directory))
                    {
                        paths.emplace_back(path);
                    }
                }
            }

        private:
            std::string_view pathOf(const PakEntry& entry) const
            {
                return std::string_view(m_paths).substr(entry.pathOffset, entry.pathBytes);
            }

            const PakEntry* find(const std::string& path) const
            {
                const uint64_t key = pathKey(path);

                for (auto entry = std::lower_bound(m_entries.begin(), m_entries.end(), key, entryBefore);
                     entry != m_entries.end() && entry->pathKey == key; ++entry)
                {
                    if (pathOf(*entry) == path)
                    {
                        return &*entry;
                    }
                }

                return nullptr;
            }

            Result<std::vector<std::byte>> readStored(const PakEntry& entry) const
            {
                std::vector<std::byte> stored(static_cast<std::size_t>(entry.storedBytes));
                std::lock_guard lock(m_fileMutex);
                m_file.clear();
                m_file.seekg(static_cast<std::streamoff>(entry.offset));

                if (!m_file.read(reinterpret_cast<char*>(stored.data()), static_cast<std::streamsize>(stored.size())))
                {
                    return pakError(m_source, "cannot read '" + std::string(pathOf(entry)) + "'");
                }

                return stored;
            }

            Result<std::vector<std::byte>> unpack(const PakEntry& entry, std::vector<std::byte> stored) const
            {
                if (entry.compression == static_cast<uint32_t>(PakCompression::None))
                {
                    return stored;
                }

                std::vector<std::byte> contents(static_cast<std::size_t>(entry.bytes));
                const std::size_t written = ZSTD_decompress(contents.data(), contents.size(), stored.data(),
                    stored.size());

                if (ZSTD_isError(written) != 0 || written != contents.size())
                {
                    return pakError(m_source, "cannot decompress '" + std::string(pathOf(entry)) + "'");
                }

                return contents;
            }

            std::filesystem::path m_source;
            mutable std::mutex m_fileMutex;
            mutable std::ifstream m_file;
            std::vector<PakEntry> m_entries;
            std::string m_paths;
        };

        Status validateEntries(const std::filesystem::path& pak, std::span<const PakEntry> entries,
            std::string_view paths, uint64_t tocOffset)
        {
            for (std::size_t index = 0; index < entries.size(); ++index)
            {
                const PakEntry& entry = entries[index];

                if (static_cast<uint64_t>(entry.pathOffset) + entry.pathBytes > paths.size()
                    || entry.offset + entry.storedBytes > tocOffset || entry.offset % k_pakAlignment != 0
                    || entry.compression > static_cast<uint32_t>(PakCompression::Zstd)
                    || (entry.compression == static_cast<uint32_t>(PakCompression::None) && entry.bytes != entry.storedBytes)
                    || entry.pathKey != pathKey(paths.substr(entry.pathOffset, entry.pathBytes))
                    || (index > 0 && entries[index - 1].pathKey > entry.pathKey))
                {
                    return pakError(pak, "table of contents entry " + std::to_string(index) + " is damaged");
                }
            }

            return {};
        }
    } //namespace

    Result<std::unique_ptr<Mount>> openPakMount(const std::filesystem::path& pak, MountInfo& info)
    {
        std::ifstream file(pak, std::ios::binary | std::ios::ate);

        if (!file)
        {
            return pakError(pak, "cannot open");
        }

        const auto fileBytes = static_cast<uint64_t>(file.tellg());
        PakHeader header{};
        file.seekg(0);

        if (fileBytes < sizeof(header) || !file.read(reinterpret_cast<char*>(&header), sizeof(header))
            || header.magic != k_pakMagic)
        {
            return pakError(pak, "not a pak");
        }

        if (header.version != k_pakVersion)
        {
            return pakError(pak, "version " + std::to_string(header.version) + " is not supported (expected "
                + std::to_string(k_pakVersion) + ")");
        }

        const uint64_t entryBytes = static_cast<uint64_t>(header.fileCount) * sizeof(PakEntry);

        if (header.tocOffset < sizeof(header) || header.tocOffset > fileBytes
            || header.tocBytes != fileBytes - header.tocOffset || header.tocBytes < entryBytes)
        {
            return pakError(pak, "table of contents is out of range");
        }

        std::vector<std::byte> toc(static_cast<std::size_t>(header.tocBytes));
        file.seekg(static_cast<std::streamoff>(header.tocOffset));

        if (!file.read(reinterpret_cast<char*>(toc.data()), static_cast<std::streamsize>(toc.size())))
        {
            return pakError(pak, "cannot read the table of contents");
        }

        if (hash64(toc) != header.tocHash)
        {
            return pakError(pak, "table of contents does not match its hash");
        }

        std::vector<PakEntry> entries(header.fileCount);
        std::memcpy(entries.data(), toc.data(), static_cast<std::size_t>(entryBytes));
        std::string paths(reinterpret_cast<const char*>(toc.data() + entryBytes), toc.size() - entryBytes);

        if (auto valid = validateEntries(pak, entries, paths, header.tocOffset); !valid)
        {
            return valid.error();
        }

        if (pakContentHash(entries, paths) != header.contentHash)
        {
            return pakError(pak, "content hash does not match its table of contents");
        }

        info.fileCount = header.fileCount;
        info.contentHash = header.contentHash;

        return std::unique_ptr<Mount>(std::make_unique<PakMount>(pak, std::move(file), std::move(entries),
            std::move(paths)));
    }
} //namespace lockstep::assets
