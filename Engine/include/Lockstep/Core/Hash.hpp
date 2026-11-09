#ifndef LOCKSTEP_CORE_HASH_HPP
#define LOCKSTEP_CORE_HASH_HPP

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

namespace lockstep
{
    // Hashes bytes with XXH64 (the xxHash 64-bit algorithm, bit-exact with the reference
    // implementation). Fast, stable across platforms and builds; not cryptographic. Used for
    // virtual path keys and content hashes of cooked files and paks.
    // bytes:   the data.
    // seed:    starting value; different seeds give unrelated hashes.
    // Returns: the 64-bit hash.
    [[nodiscard]] uint64_t hash64(std::span<const std::byte> bytes, uint64_t seed = 0) noexcept;

    // Hashes the characters of a string with hash64().
    // text:    the string (no terminator is hashed).
    // seed:    starting value.
    // Returns: the 64-bit hash.
    [[nodiscard]] uint64_t hash64(std::string_view text, uint64_t seed = 0) noexcept;
} //namespace lockstep

#endif //LOCKSTEP_CORE_HASH_HPP
