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

    // Which texture formats decoding may produce.
    // blockCompression: the GPU samples Bc formats; without it everything becomes Rgba8.
    // compress:         block compress single-level colour images on the CPU (with CPU mips, see
    //                   buildMippedTexture()). Only honoured with blockCompression. Data images
    //                   (normal, metallic-roughness, occlusion) are never re-encoded: Bc1/Bc3
    //                   band normal maps.
    struct DecodeTargets
    {
        bool blockCompression{true};
        bool compress{true};
    };

    // Decodes an encoded image into GPU-ready texture data. Thread-safe given a shared gate.
    // KTX2 with Basis Universal (UASTC or ETC1S) and more than one level is transcoded to Bc7, or
    // Rgba8 without block compression. Every single-level image (jpg, png, single-level KTX2,
    // Basis or Rgba8) is decoded to Rgba8: colour images are then given CPU mips and Bc1/Bc3 when
    // compressing, everything else stays Rgba8 and the GPU builds its mips. KTX2 that is already
    // Bc1, Bc3 or Bc7, or Rgba8 with mips, is used as stored.
    // encoded: the file contents (jpg, png or KTX2, recognised by signature).
    // srgb:    the image holds colour (base colour, emissive) rather than data.
    // targets: which formats may be produced.
    // gate:    the import's transcoder gate.
    // Returns: the texture, or an error if the format is unknown or unsupported (including stored
    //          Bc without block compression) or decoding failed.
    [[nodiscard]] Result<DecodedTexture> decodeImage(std::span<const std::byte> encoded, bool srgb,
        const DecodeTargets& targets, TranscoderGate& gate);
} //namespace lockstep::assets

#endif //LOCKSTEP_ASSETS_IMAGE_DECODE_HPP
