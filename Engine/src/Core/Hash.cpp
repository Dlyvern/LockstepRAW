#include "Lockstep/Core/Hash.hpp"

#include <bit>
#include <cstring>

namespace lockstep
{
    namespace
    {
        constexpr uint64_t k_prime1{0x9E3779B185EBCA87ull};
        constexpr uint64_t k_prime2{0xC2B2AE3D27D4EB4Full};
        constexpr uint64_t k_prime3{0x165667B19E3779F9ull};
        constexpr uint64_t k_prime4{0x85EBCA77C2B2AE63ull};
        constexpr uint64_t k_prime5{0x27D4EB2F165667C5ull};
        constexpr std::size_t k_stripeBytes{32};

        static_assert(std::endian::native == std::endian::little, "hash64 reads little-endian words");

        uint64_t read64(const std::byte* bytes) noexcept
        {
            uint64_t value = 0;
            std::memcpy(&value, bytes, sizeof(value));
            return value;
        }

        uint32_t read32(const std::byte* bytes) noexcept
        {
            uint32_t value = 0;
            std::memcpy(&value, bytes, sizeof(value));
            return value;
        }

        uint64_t round(uint64_t accumulator, uint64_t input) noexcept
        {
            accumulator += input * k_prime2;
            accumulator = std::rotl(accumulator, 31);
            return accumulator * k_prime1;
        }

        uint64_t mergeRound(uint64_t accumulator, uint64_t value) noexcept
        {
            accumulator ^= round(0, value);
            return accumulator * k_prime1 + k_prime4;
        }

        uint64_t avalanche(uint64_t hash) noexcept
        {
            hash ^= hash >> 33;
            hash *= k_prime2;
            hash ^= hash >> 29;
            hash *= k_prime3;
            hash ^= hash >> 32;
            return hash;
        }
    } //namespace

    uint64_t hash64(std::span<const std::byte> bytes, uint64_t seed) noexcept
    {
        const std::byte* cursor = bytes.data();
        const std::byte* const end = cursor + bytes.size();
        uint64_t hash = 0;

        if (bytes.size() >= k_stripeBytes)
        {
            uint64_t lane1 = seed + k_prime1 + k_prime2;
            uint64_t lane2 = seed + k_prime2;
            uint64_t lane3 = seed;
            uint64_t lane4 = seed - k_prime1;
            const std::byte* const lastStripe = end - k_stripeBytes;

            while (cursor <= lastStripe)
            {
                lane1 = round(lane1, read64(cursor));
                lane2 = round(lane2, read64(cursor + 8));
                lane3 = round(lane3, read64(cursor + 16));
                lane4 = round(lane4, read64(cursor + 24));
                cursor += k_stripeBytes;
            }

            hash = std::rotl(lane1, 1) + std::rotl(lane2, 7) + std::rotl(lane3, 12) + std::rotl(lane4, 18);
            hash = mergeRound(hash, lane1);
            hash = mergeRound(hash, lane2);
            hash = mergeRound(hash, lane3);
            hash = mergeRound(hash, lane4);
        }
        else
        {
            hash = seed + k_prime5;
        }

        hash += static_cast<uint64_t>(bytes.size());

        while (end - cursor >= 8)
        {
            hash ^= round(0, read64(cursor));
            hash = std::rotl(hash, 27) * k_prime1 + k_prime4;
            cursor += 8;
        }

        if (end - cursor >= 4)
        {
            hash ^= static_cast<uint64_t>(read32(cursor)) * k_prime1;
            hash = std::rotl(hash, 23) * k_prime2 + k_prime3;
            cursor += 4;
        }

        while (cursor < end)
        {
            hash ^= static_cast<uint64_t>(std::to_integer<uint8_t>(*cursor)) * k_prime5;
            hash = std::rotl(hash, 11) * k_prime1;
            ++cursor;
        }

        return avalanche(hash);
    }

    uint64_t hash64(std::string_view text, uint64_t seed) noexcept
    {
        return hash64(std::as_bytes(std::span<const char>(text.data(), text.size())), seed);
    }
} //namespace lockstep
