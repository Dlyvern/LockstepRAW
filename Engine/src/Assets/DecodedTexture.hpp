#ifndef LOCKSTEP_ASSETS_DECODED_TEXTURE_HPP
#define LOCKSTEP_ASSETS_DECODED_TEXTURE_HPP

#include "Lockstep/Render/Resources.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace lockstep::assets
{
    // A texture decoded on the CPU, laid out as render::TextureDescription expects: every mip
    // level, largest first, tightly packed. With generateMips, `bytes` holds one Rgba8 level and
    // the GPU builds the rest.
    struct DecodedTexture
    {
        render::TextureFormat format{render::TextureFormat::Rgba8Srgb};
        uint32_t width{0};
        uint32_t height{0};
        uint32_t levels{1};
        bool generateMips{false};
        std::vector<std::byte> bytes;
    };

    // Returns: true for the Bc formats.
    [[nodiscard]] bool isBlockCompressed(render::TextureFormat format) noexcept;
} //namespace lockstep::assets

#endif //LOCKSTEP_ASSETS_DECODED_TEXTURE_HPP
