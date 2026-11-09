#include "BlockCompression.hpp"

#include <algorithm>
#include <array>
#include <cstring>

#define STB_DXT_IMPLEMENTATION
#include <stb_dxt.h>

namespace lockstep::assets
{
    namespace
    {
        constexpr uint32_t k_blockSize{4};
        constexpr std::size_t k_channels{4};
        constexpr std::size_t k_bc1BlockBytes{8};
        constexpr std::size_t k_bc3BlockBytes{16};
    } //namespace

    void appendBlockCompressed(std::span<const uint8_t> rgba, uint32_t width, uint32_t height, bool withAlpha,
        std::vector<std::byte>& output)
    {
        const uint32_t blocksX = (width + k_blockSize - 1) / k_blockSize;
        const uint32_t blocksY = (height + k_blockSize - 1) / k_blockSize;
        const std::size_t blockBytes = withAlpha ? k_bc3BlockBytes : k_bc1BlockBytes;
        const std::size_t start = output.size();
        output.resize(start + static_cast<std::size_t>(blocksX) * blocksY * blockBytes);

        std::array<uint8_t, k_blockSize * k_blockSize * k_channels> block{};
        auto* destination = reinterpret_cast<unsigned char*>(output.data() + start);

        for (uint32_t blockY = 0; blockY < blocksY; ++blockY)
        {
            for (uint32_t blockX = 0; blockX < blocksX; ++blockX)
            {
                for (uint32_t row = 0; row < k_blockSize; ++row)
                {
                    const uint32_t y = std::min(blockY * k_blockSize + row, height - 1);

                    for (uint32_t column = 0; column < k_blockSize; ++column)
                    {
                        const uint32_t x = std::min(blockX * k_blockSize + column, width - 1);
                        const std::size_t source = (static_cast<std::size_t>(y) * width + x) * k_channels;
                        std::copy_n(rgba.data() + source, k_channels,
                            block.data() + (row * k_blockSize + column) * k_channels);
                    }
                }

                stb_compress_dxt_block(destination, block.data(), withAlpha ? 1 : 0, STB_DXT_NORMAL);
                destination += blockBytes;
            }
        }
    }
} //namespace lockstep::assets
