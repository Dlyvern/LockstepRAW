#include "TextureCook.hpp"

#include "Assets/CookedFormat.hpp"
#include "Assets/MipChain.hpp"

#include "BlockEncoders.hpp"

#include <algorithm>
#include <cmath>

namespace lockstep::cooker
{
    namespace
    {
        constexpr std::size_t k_channels{4};

        float toSigned(uint8_t value)
        {
            return static_cast<float>(value) / 127.5f - 1.0f;
        }

        uint8_t toUnsigned(float value)
        {
            return static_cast<uint8_t>(std::clamp((value + 1.0f) * 127.5f + 0.5f, 0.0f, 255.0f));
        }

        math::Vec3 readNormal(const uint8_t* texel)
        {
            const float x = toSigned(texel[0]);
            const float y = toSigned(texel[1]);
            return math::Vec3{x, y, std::sqrt(std::max(1.0f - x * x - y * y, 0.0f))};
        }

        void writeNormal(uint8_t* texel, const math::Vec3& normal)
        {
            const float length = math::length(normal);
            const math::Vec3 unit = length > 1.0e-6f ? normal / length : math::Vec3{0.0f, 0.0f, 1.0f};
            texel[0] = toUnsigned(unit.x);
            texel[1] = toUnsigned(unit.y);
            texel[2] = toUnsigned(unit.z);
            texel[3] = 255;
        }

        std::vector<uint8_t> downsampleNormals(std::span<const uint8_t> source, uint32_t width, uint32_t height)
        {
            const uint32_t nextWidth = std::max(1u, width / 2);
            const uint32_t nextHeight = std::max(1u, height / 2);
            std::vector<uint8_t> result(static_cast<std::size_t>(nextWidth) * nextHeight * k_channels);

            for (uint32_t y = 0; y < nextHeight; ++y)
            {
                for (uint32_t x = 0; x < nextWidth; ++x)
                {
                    math::Vec3 sum{0.0f, 0.0f, 0.0f};

                    for (uint32_t tap = 0; tap < 4; ++tap)
                    {
                        const uint32_t row = std::min(y * 2 + tap / 2, height - 1);
                        const uint32_t column = std::min(x * 2 + tap % 2, width - 1);
                        sum = sum + readNormal(source.data() + (static_cast<std::size_t>(row) * width + column) * k_channels);
                    }

                    writeNormal(result.data() + (static_cast<std::size_t>(y) * nextWidth + x) * k_channels, sum);
                }
            }

            return result;
        }

        std::vector<std::vector<uint8_t>> normalMipChain(std::span<const uint8_t> rgba, uint32_t width, uint32_t height)
        {
            std::vector<std::vector<uint8_t>> levels;
            levels.emplace_back(rgba.begin(), rgba.end());

            for (uint32_t level = 1; level < render::fullMipLevelCount(width, height); ++level)
            {
                const uint32_t levelWidth = std::max(1u, width >> (level - 1));
                const uint32_t levelHeight = std::max(1u, height >> (level - 1));
                levels.push_back(downsampleNormals(levels.back(), levelWidth, levelHeight));
            }

            return levels;
        }

        std::vector<std::vector<uint8_t>> mipChain(std::span<const uint8_t> rgba, uint32_t width, uint32_t height,
            TextureUsage usage)
        {
            if (usage == TextureUsage::Normal)
            {
                return normalMipChain(rgba, width, height);
            }

            const assets::DecodedTexture chain = assets::buildMippedTexture(rgba, width, height,
                usage == TextureUsage::Colour, false);
            std::vector<std::vector<uint8_t>> levels;
            std::size_t offset = 0;

            for (uint32_t level = 0; level < chain.levels; ++level)
            {
                const std::size_t size = render::textureLevelBytes(chain.format, std::max(1u, width >> level),
                    std::max(1u, height >> level));
                const auto* begin = reinterpret_cast<const uint8_t*>(chain.bytes.data() + offset);
                levels.emplace_back(begin, begin + size);
                offset += size;
            }

            return levels;
        }
    } //namespace

    render::TextureFormat cookedFormat(TextureUsage usage) noexcept
    {
        switch (usage)
        {
            case TextureUsage::Normal:
            {
                return render::TextureFormat::Bc5Unorm;
            }
            case TextureUsage::Data:
            {
                return render::TextureFormat::Bc7Unorm;
            }
            case TextureUsage::Occlusion:
            {
                return render::TextureFormat::Bc4Unorm;
            }
            default:
            {
                return render::TextureFormat::Bc7Srgb;
            }
        }
    }

    Result<CookedTexture> cookTexture(std::span<const std::byte> encoded, TextureUsage usage, uint32_t maxSize,
        assets::TranscoderGate& gate)
    {
        const assets::DecodeTargets rgba8Only{.blockCompression = false, .compress = false};
        auto decoded = assets::decodeImage(encoded, usage == TextureUsage::Colour, rgba8Only, gate);

        if (!decoded)
        {
            return decoded.error();
        }

        const assets::DecodedTexture& source = decoded.value();
        const std::size_t baseBytes = static_cast<std::size_t>(source.width) * source.height * k_channels;
        const std::span<const uint8_t> base(reinterpret_cast<const uint8_t*>(source.bytes.data()), baseBytes);
        const std::vector<std::vector<uint8_t>> levels = mipChain(base, source.width, source.height, usage);

        CookedTexture cooked{};
        cooked.format = cookedFormat(usage);
        cooked.sourceWidth = source.width;
        cooked.sourceHeight = source.height;
        uint32_t topLevel = 0;

        while (topLevel + 1 < levels.size()
            && std::max(source.width >> topLevel, source.height >> topLevel) > std::max(maxSize, 1u))
        {
            ++topLevel;
        }

        for (uint32_t level = 0; level < levels.size(); ++level)
        {
            cooked.uncappedBytes += render::textureLevelBytes(cooked.format, std::max(1u, source.width >> level),
                std::max(1u, source.height >> level));
        }

        cooked.width = std::max(1u, source.width >> topLevel);
        cooked.height = std::max(1u, source.height >> topLevel);
        cooked.levels = static_cast<uint32_t>(levels.size()) - topLevel;
        std::vector<std::byte> pixels;

        for (uint32_t level = 0; level < cooked.levels; ++level)
        {
            const std::size_t start = pixels.size();
            const uint32_t levelWidth = std::max(1u, cooked.width >> level);
            const uint32_t levelHeight = std::max(1u, cooked.height >> level);
            encodeBlocks(cooked.format, levels[topLevel + level], levelWidth, levelHeight, pixels);

            if (level == 0)
            {
                cooked.meanSquaredError = blockError(cooked.format, std::span(pixels).subspan(start),
                    levels[topLevel], levelWidth, levelHeight);
            }
        }

        cooked.pixelBytes = pixels.size();
        cooked.file = assets::writeCookedTexture(assets::CookedTextureHeader{
            .format = static_cast<uint32_t>(cooked.format),
            .width = cooked.width,
            .height = cooked.height,
            .levels = cooked.levels}, pixels);

        return cooked;
    }
} //namespace lockstep::cooker
