#ifndef LOCKSTEP_ASSETS_BLOCK_COMPRESSION_HPP
#define LOCKSTEP_ASSETS_BLOCK_COMPRESSION_HPP

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace lockstep::assets
{
    // Compresses one RGBA8 image into BC1 (8 bytes per 4x4 block, alpha dropped) or BC3 (16
    // bytes per block). Edge blocks repeat the last row and column.
    // rgba:      width * height * 4 bytes.
    // width:     pixels per row.
    // height:    rows.
    // withAlpha: BC3 instead of BC1.
    // output:    receives the blocks, appended.
    void appendBlockCompressed(std::span<const uint8_t> rgba, uint32_t width, uint32_t height, bool withAlpha,
        std::vector<std::byte>& output);
} //namespace lockstep::assets

#endif //LOCKSTEP_ASSETS_BLOCK_COMPRESSION_HPP
