#include "BlockEncoders.hpp"

#include <bc7decomp.h>
#include <bc7enc.h>
#include <rgbcx.h>

#include <algorithm>
#include <array>
#include <mutex>

namespace lockstep::cooker
{
    namespace
    {
        constexpr uint32_t k_blockSize{4};
        constexpr std::size_t k_channels{4};
        constexpr uint32_t k_bc7UberLevel{1};

        using Block = std::array<uint8_t, k_blockSize * k_blockSize * k_channels>;

        void initializeEncoders()
        {
            static std::once_flag once;
            std::call_once(once, []
            {
                bc7enc_compress_block_init();
                rgbcx::init();
            });
        }

        std::size_t bytesPerBlock(render::TextureFormat format)
        {
            return format == render::TextureFormat::Bc4Unorm ? 8 : 16;
        }

        uint32_t channelsKept(render::TextureFormat format)
        {
            switch (format)
            {
                case render::TextureFormat::Bc4Unorm:
                {
                    return 1;
                }
                case render::TextureFormat::Bc5Unorm:
                {
                    return 2;
                }
                default:
                {
                    return 3;
                }
            }
        }

        Block gatherBlock(std::span<const uint8_t> rgba, uint32_t width, uint32_t height, uint32_t blockX, uint32_t blockY)
        {
            Block block{};

            for (uint32_t row = 0; row < k_blockSize; ++row)
            {
                const uint32_t y = std::min(blockY * k_blockSize + row, height - 1);

                for (uint32_t column = 0; column < k_blockSize; ++column)
                {
                    const uint32_t x = std::min(blockX * k_blockSize + column, width - 1);
                    const std::size_t source = (static_cast<std::size_t>(y) * width + x) * k_channels;
                    std::copy_n(rgba.data() + source, k_channels, block.data() + (row * k_blockSize + column) * k_channels);
                }
            }

            return block;
        }

        bc7enc_compress_block_params bc7Params(bool perceptual)
        {
            bc7enc_compress_block_params params{};
            bc7enc_compress_block_params_init(&params);
            params.m_uber_level = k_bc7UberLevel;

            if (!perceptual)
            {
                bc7enc_compress_block_params_init_linear_weights(&params);
            }

            return params;
        }

        void encodeBlock(render::TextureFormat format, const bc7enc_compress_block_params& params, const Block& block,
            std::byte* destination)
        {
            switch (format)
            {
                case render::TextureFormat::Bc4Unorm:
                {
                    rgbcx::encode_bc4_hq(destination, block.data());
                    break;
                }
                case render::TextureFormat::Bc5Unorm:
                {
                    rgbcx::encode_bc5_hq(destination, block.data());
                    break;
                }
                default:
                {
                    bc7enc_compress_block(destination, block.data(), &params);
                    break;
                }
            }
        }

        Block decodeBlock(render::TextureFormat format, const std::byte* source)
        {
            Block block{};

            switch (format)
            {
                case render::TextureFormat::Bc4Unorm:
                {
                    rgbcx::unpack_bc4(source, block.data());
                    break;
                }
                case render::TextureFormat::Bc5Unorm:
                {
                    rgbcx::unpack_bc5(source, block.data());
                    break;
                }
                default:
                {
                    bc7decomp::unpack_bc7(source, reinterpret_cast<bc7decomp::color_rgba*>(block.data()));
                    break;
                }
            }

            return block;
        }
    } //namespace

    void encodeBlocks(render::TextureFormat format, std::span<const uint8_t> rgba, uint32_t width, uint32_t height,
        std::vector<std::byte>& output)
    {
        initializeEncoders();

        const uint32_t blocksX = (width + k_blockSize - 1) / k_blockSize;
        const uint32_t blocksY = (height + k_blockSize - 1) / k_blockSize;
        const std::size_t blockBytes = bytesPerBlock(format);
        const std::size_t start = output.size();
        output.resize(start + static_cast<std::size_t>(blocksX) * blocksY * blockBytes);
        const bc7enc_compress_block_params params = bc7Params(format == render::TextureFormat::Bc7Srgb);
        std::byte* destination = output.data() + start;

        for (uint32_t blockY = 0; blockY < blocksY; ++blockY)
        {
            for (uint32_t blockX = 0; blockX < blocksX; ++blockX)
            {
                encodeBlock(format, params, gatherBlock(rgba, width, height, blockX, blockY), destination);
                destination += blockBytes;
            }
        }
    }

    double blockError(render::TextureFormat format, std::span<const std::byte> blocks, std::span<const uint8_t> rgba,
        uint32_t width, uint32_t height)
    {
        const uint32_t blocksX = (width + k_blockSize - 1) / k_blockSize;
        const std::size_t blockBytes = bytesPerBlock(format);
        const uint32_t channels = channelsKept(format);
        double squaredError = 0.0;

        for (uint32_t y = 0; y < height; y += k_blockSize)
        {
            for (uint32_t x = 0; x < width; x += k_blockSize)
            {
                const std::size_t index = static_cast<std::size_t>(y / k_blockSize) * blocksX + x / k_blockSize;
                const Block decoded = decodeBlock(format, blocks.data() + index * blockBytes);

                for (uint32_t row = 0; row < k_blockSize && y + row < height; ++row)
                {
                    for (uint32_t column = 0; column < k_blockSize && x + column < width; ++column)
                    {
                        const std::size_t source = (static_cast<std::size_t>(y + row) * width + x + column) * k_channels;
                        const std::size_t target = (row * k_blockSize + column) * k_channels;

                        for (uint32_t channel = 0; channel < channels; ++channel)
                        {
                            const double difference = static_cast<double>(decoded[target + channel])
                                - static_cast<double>(rgba[source + channel]);
                            squaredError += difference * difference;
                        }
                    }
                }
            }
        }

        return squaredError / (static_cast<double>(width) * height * channels);
    }
} //namespace lockstep::cooker
