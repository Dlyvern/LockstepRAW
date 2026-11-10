#ifndef LOCKSTEP_RENDER_PNG_WRITER_HPP
#define LOCKSTEP_RENDER_PNG_WRITER_HPP

#include "Lockstep/Core/Result.hpp"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

namespace lockstep::render
{
    // Writes an RGBA8 image as an uncompressed (stored deflate) RGB PNG, dropping alpha. For
    // dev captures, so simplicity beats file size.
    // path:   file to write (overwritten).
    // pixels: width * height * 4 bytes, rows top to bottom, already sRGB-encoded.
    // width, height: the image size in pixels.
    // Returns: an error if the sizes do not match or the file cannot be written.
    [[nodiscard]] Status writePng(std::string_view path, std::span<const std::byte> pixels, uint32_t width,
        uint32_t height);
} //namespace lockstep::render

#endif //LOCKSTEP_RENDER_PNG_WRITER_HPP
