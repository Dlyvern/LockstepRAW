#ifndef LOCKSTEP_GAME_PLAYER_WIRE_BYTES_HPP
#define LOCKSTEP_GAME_PLAYER_WIRE_BYTES_HPP

#include <bit>
#include <cstddef>
#include <cstdint>
#include <span>

namespace game
{
    // Writes a float into a message as 4 little-endian bytes.
    // bytes:  the message.
    // offset: where the float starts; `offset + 4` must not exceed the size.
    // value:  the float.
    inline void writeFloat(std::span<std::byte> bytes, std::size_t offset, float value)
    {
        const auto bits = std::bit_cast<uint32_t>(value);

        for (std::size_t index = 0; index < sizeof(uint32_t); ++index)
        {
            bytes[offset + index] = static_cast<std::byte>((bits >> (8 * index)) & 0xFFu);
        }
    }

    // Reads a float written by writeFloat().
    // bytes:   the message.
    // offset:  where the float starts; `offset + 4` must not exceed the size.
    // Returns: the float.
    [[nodiscard]] inline float readFloat(std::span<const std::byte> bytes, std::size_t offset)
    {
        uint32_t bits = 0;

        for (std::size_t index = 0; index < sizeof(uint32_t); ++index)
        {
            bits |= static_cast<uint32_t>(bytes[offset + index]) << (8 * index);
        }

        return std::bit_cast<float>(bits);
    }
} //namespace game

#endif //LOCKSTEP_GAME_PLAYER_WIRE_BYTES_HPP
