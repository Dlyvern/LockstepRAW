#include "PakFormat.hpp"

#include "Lockstep/Core/Hash.hpp"

#include <vector>

namespace lockstep::assets
{
    uint64_t pakContentHash(std::span<const PakEntry> entries, std::string_view paths) noexcept
    {
        std::vector<std::byte> record;
        uint64_t hash = 0;

        for (const PakEntry& entry : entries)
        {
            const std::string_view path = paths.substr(entry.pathOffset, entry.pathBytes);
            const auto* hashBytes = reinterpret_cast<const std::byte*>(&entry.contentHash);
            const auto* pathBytes = reinterpret_cast<const std::byte*>(path.data());
            record.assign(pathBytes, pathBytes + path.size());
            record.push_back(std::byte{0});
            record.insert(record.end(), hashBytes, hashBytes + sizeof(entry.contentHash));
            hash = hash64(record, hash);
        }

        return hash;
    }
} //namespace lockstep::assets
