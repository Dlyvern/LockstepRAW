#ifndef LOCKSTEP_ASSETS_MIP_CHAIN_HPP
#define LOCKSTEP_ASSETS_MIP_CHAIN_HPP

#include "DecodedTexture.hpp"

#include <cstdint>
#include <span>

namespace lockstep::assets
{
    // Builds a texture with a full mip chain from 8-bit RGBA pixels. Each level is a 2x2 box
    // filter of the one above, averaged in linear space for sRGB data.
    // rgba:     width * height * 4 bytes, row by row from the top.
    // width:    pixels per row.
    // height:   rows.
    // srgb:     the data is sRGB colour (selects the Srgb formats and linear-space filtering).
    // compress: store as Bc1, or Bc3 if any pixel is not fully opaque, instead of Rgba8.
    // Returns:  the texture.
    [[nodiscard]] DecodedTexture buildMippedTexture(std::span<const uint8_t> rgba, uint32_t width, uint32_t height,
        bool srgb, bool compress);
} //namespace lockstep::assets

#endif //LOCKSTEP_ASSETS_MIP_CHAIN_HPP
