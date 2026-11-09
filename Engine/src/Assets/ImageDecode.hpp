#ifndef LOCKSTEP_ASSETS_IMAGE_DECODE_HPP
#define LOCKSTEP_ASSETS_IMAGE_DECODE_HPP

#include "Lockstep/Core/Result.hpp"

#include "DecodedTexture.hpp"

#include <atomic>
#include <cstddef>
#include <mutex>
#include <span>

namespace lockstep::assets
{
    // Serialises the first Basis Universal transcode of an import: libktx initialises the
    // transcoder's global tables lazily and without a lock. Shared by all decoding threads.
    struct TranscoderGate
    {
        std::mutex mutex;
        std::atomic<bool> isInitialized{false};
    };

    // Decodes an encoded image into GPU-ready texture data. Thread-safe given a shared gate.
    // KTX2 with Basis Universal (UASTC or ETC1S) and more than one level is transcoded to Bc7;
    // with one level it is transcoded to RGBA and given mips like jpg/png. KTX2 that is already
    // Rgba8, Bc1, Bc3 or Bc7 is used as stored, and gets mips if it is Rgba8 with one level.
    // encoded:  the file contents (jpg, png or KTX2, recognised by signature).
    // srgb:     the image holds colour (base colour, emissive) rather than data.
    // compress: block compress images that arrive uncompressed (see buildMippedTexture()).
    // gate:     the import's transcoder gate.
    // Returns:  the texture, or an error if the format is unknown or unsupported or decoding failed.
    [[nodiscard]] Result<DecodedTexture> decodeImage(std::span<const std::byte> encoded, bool srgb, bool compress,
        TranscoderGate& gate);
} //namespace lockstep::assets

#endif //LOCKSTEP_ASSETS_IMAGE_DECODE_HPP
