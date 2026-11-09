#include "MipChain.hpp"

#include "BlockCompression.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <vector>

namespace lockstep::assets
{
    namespace
    {
        constexpr std::size_t k_channels{4};
        constexpr std::size_t k_alphaChannel{3};
        constexpr std::size_t k_linearToSrgbSteps{4096};

        struct ColourTables
        {
            std::array<float, 256> toLinear{};
            std::array<uint8_t, k_linearToSrgbSteps + 1> toSrgb{};
        };

        float srgbToLinear(float value)
        {
            return value <= 0.04045f ? value / 12.92f : std::pow((value + 0.055f) / 1.055f, 2.4f);
        }

        float linearToSrgb(float value)
        {
            return value <= 0.0031308f ? value * 12.92f : 1.055f * std::pow(value, 1.0f / 2.4f) - 0.055f;
        }

        uint8_t toByte(float value)
        {
            return static_cast<uint8_t>(std::clamp(value * 255.0f + 0.5f, 0.0f, 255.0f));
        }

        ColourTables makeColourTables()
        {
            ColourTables tables{};

            for (std::size_t index = 0; index < tables.toLinear.size(); ++index)
            {
                tables.toLinear[index] = srgbToLinear(static_cast<float>(index) / 255.0f);
            }

            for (std::size_t index = 0; index < tables.toSrgb.size(); ++index)
            {
                tables.toSrgb[index] = toByte(linearToSrgb(static_cast<float>(index) / k_linearToSrgbSteps));
            }

            return tables;
        }

        const ColourTables& colourTables()
        {
            static const ColourTables tables = makeColourTables();
            return tables;
        }

        std::vector<uint8_t> downsample(std::span<const uint8_t> source, uint32_t width, uint32_t height, bool srgb)
        {
            const ColourTables& tables = colourTables();
            const uint32_t nextWidth = std::max(1u, width / 2);
            const uint32_t nextHeight = std::max(1u, height / 2);
            std::vector<uint8_t> result(static_cast<std::size_t>(nextWidth) * nextHeight * k_channels);

            for (uint32_t y = 0; y < nextHeight; ++y)
            {
                const uint32_t row0 = std::min(y * 2, height - 1);
                const uint32_t row1 = std::min(y * 2 + 1, height - 1);

                for (uint32_t x = 0; x < nextWidth; ++x)
                {
                    const uint32_t column0 = std::min(x * 2, width - 1);
                    const uint32_t column1 = std::min(x * 2 + 1, width - 1);
                    const std::array<std::size_t, 4> taps{
                        (static_cast<std::size_t>(row0) * width + column0) * k_channels,
                        (static_cast<std::size_t>(row0) * width + column1) * k_channels,
                        (static_cast<std::size_t>(row1) * width + column0) * k_channels,
                        (static_cast<std::size_t>(row1) * width + column1) * k_channels};
                    uint8_t* destination = result.data() + (static_cast<std::size_t>(y) * nextWidth + x) * k_channels;

                    for (std::size_t channel = 0; channel < k_channels; ++channel)
                    {
                        const bool isColour = srgb && channel != k_alphaChannel;
                        float sum = 0.0f;

                        for (const std::size_t tap : taps)
                        {
                            const uint8_t value = source[tap + channel];
                            sum += isColour ? tables.toLinear[value] : static_cast<float>(value);
                        }

                        const float average = sum * 0.25f;
                        destination[channel] = isColour
                            ? tables.toSrgb[static_cast<std::size_t>(average * k_linearToSrgbSteps + 0.5f)]
                            : static_cast<uint8_t>(average + 0.5f);
                    }
                }
            }

            return result;
        }

        bool hasTransparency(std::span<const uint8_t> rgba)
        {
            for (std::size_t index = k_alphaChannel; index < rgba.size(); index += k_channels)
            {
                if (rgba[index] != 255)
                {
                    return true;
                }
            }

            return false;
        }

        render::TextureFormat chooseFormat(bool srgb, bool compress, bool withAlpha)
        {
            if (!compress)
            {
                return srgb ? render::TextureFormat::Rgba8Srgb : render::TextureFormat::Rgba8Unorm;
            }

            if (withAlpha)
            {
                return srgb ? render::TextureFormat::Bc3Srgb : render::TextureFormat::Bc3Unorm;
            }

            return srgb ? render::TextureFormat::Bc1Srgb : render::TextureFormat::Bc1Unorm;
        }

        void appendLevel(DecodedTexture& texture, std::span<const uint8_t> rgba, uint32_t width, uint32_t height,
            bool compress, bool withAlpha)
        {
            if (compress)
            {
                appendBlockCompressed(rgba, width, height, withAlpha, texture.bytes);
                return;
            }

            const std::size_t start = texture.bytes.size();
            texture.bytes.resize(start + rgba.size());
            std::memcpy(texture.bytes.data() + start, rgba.data(), rgba.size());
        }
    } //namespace

    DecodedTexture buildMippedTexture(std::span<const uint8_t> rgba, uint32_t width, uint32_t height, bool srgb,
        bool compress)
    {
        const bool withAlpha = hasTransparency(rgba);

        DecodedTexture texture{};
        texture.format = chooseFormat(srgb, compress, withAlpha);
        texture.width = width;
        texture.height = height;
        texture.levels = render::fullMipLevelCount(width, height);

        std::size_t totalBytes = 0;

        for (uint32_t level = 0; level < texture.levels; ++level)
        {
            totalBytes += render::textureLevelBytes(texture.format, std::max(1u, width >> level),
                std::max(1u, height >> level));
        }

        texture.bytes.reserve(totalBytes);
        appendLevel(texture, rgba, width, height, compress, withAlpha);

        std::vector<uint8_t> current;
        std::span<const uint8_t> previous = rgba;
        uint32_t levelWidth = width;
        uint32_t levelHeight = height;

        for (uint32_t level = 1; level < texture.levels; ++level)
        {
            current = downsample(previous, levelWidth, levelHeight, srgb);
            levelWidth = std::max(1u, levelWidth / 2);
            levelHeight = std::max(1u, levelHeight / 2);
            appendLevel(texture, current, levelWidth, levelHeight, compress, withAlpha);
            previous = current;
        }

        return texture;
    }
} //namespace lockstep::assets
