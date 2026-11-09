#ifndef LOCKSTEP_COOKER_BLOCK_ENCODERS_HPP
#define LOCKSTEP_COOKER_BLOCK_ENCODERS_HPP

#include <Lockstep/Render/Resources.hpp>

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace lockstep::cooker
{
    // Compresses one RGBA8 image into a Bc format with high-quality offline encoders: bc7enc
    // (Bc7Srgb with perceptual weights, Bc7Unorm with linear weights) and rgbcx (Bc5Unorm from R
    // and G, Bc4Unorm from R, both with the exhaustive "hq" search). Deterministic and
    // thread-safe. Edge blocks repeat the last row and column.
    // format: Bc7Srgb, Bc7Unorm, Bc5Unorm or Bc4Unorm.
    // rgba:   width * height * 4 bytes, rows from the top.
    // width:  pixels per row.
    // height: rows.
    // output: receives the blocks, appended.
    void encodeBlocks(render::TextureFormat format, std::span<const uint8_t> rgba, uint32_t width, uint32_t height,
        std::vector<std::byte>& output);

    // Decodes blocks made by encodeBlocks() and compares them with the source.
    // format:  the blocks' format.
    // blocks:  one level of blocks.
    // rgba:    the source pixels.
    // width:   pixels per row.
    // height:  rows.
    // Returns: mean squared error per channel over the channels the format keeps (RGB for Bc7,
    //          RG for Bc5, R for Bc4), in 8-bit units.
    [[nodiscard]] double blockError(render::TextureFormat format, std::span<const std::byte> blocks,
        std::span<const uint8_t> rgba, uint32_t width, uint32_t height);
} //namespace lockstep::cooker

#endif //LOCKSTEP_COOKER_BLOCK_ENCODERS_HPP
