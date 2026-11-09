#ifndef LOCKSTEP_COOKER_TEXTURE_COOK_HPP
#define LOCKSTEP_COOKER_TEXTURE_COOK_HPP

#include <Lockstep/Core/Result.hpp>
#include <Lockstep/Render/Resources.hpp>

#include "Assets/ImageDecode.hpp"

#include "SceneSource.hpp"

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace lockstep::cooker
{
    // A texture in its final GPU form.
    // file:             the .tex file contents.
    // format:           the block format chosen for the usage.
    // width / height:   base level size.
    // levels:           mip levels (always the full chain).
    // pixelBytes:       GPU memory of all levels.
    // meanSquaredError: encoding error of the base level against the decoded source (see
    //                   blockError()).
    struct CookedTexture
    {
        std::vector<std::byte> file;
        render::TextureFormat format{render::TextureFormat::Bc7Srgb};
        uint32_t width{0};
        uint32_t height{0};
        uint32_t levels{0};
        uint64_t pixelBytes{0};
        double meanSquaredError{0.0};
    };

    // usage:   what the texture holds.
    // Returns: the block format it is cooked to (Bc7Srgb, Bc5Unorm, Bc7Unorm or Bc4Unorm).
    [[nodiscard]] render::TextureFormat cookedFormat(TextureUsage usage) noexcept;

    // Cooks an encoded image: decodes it (jpg, png, KTX2 incl. Basis Universal; a stored mip
    // chain is replaced), builds a full mip chain on the CPU (box filter; sRGB colour averaged in
    // linear space; normal maps averaged as unit vectors and renormalized) and block-compresses
    // every level. Deterministic.
    // encoded: the image file contents.
    // usage:   what it holds, which picks the format.
    // gate:    the cook's shared Basis transcoder gate.
    // Returns: the cooked texture, or the decode error.
    [[nodiscard]] Result<CookedTexture> cookTexture(std::span<const std::byte> encoded, TextureUsage usage,
        assets::TranscoderGate& gate);
} //namespace lockstep::cooker

#endif //LOCKSTEP_COOKER_TEXTURE_COOK_HPP
