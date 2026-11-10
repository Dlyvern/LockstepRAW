#include "Lockstep/Render/Renderer.hpp"

#include "RendererState.hpp"
#include "VelaConversions.hpp"

#include <Vela/Graphics/ImageData.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace lockstep::render
{
    namespace
    {
        constexpr uint32_t k_minDroppedSide{128};
        constexpr uint32_t k_rgbaChannels{4};

        uint32_t droppableLevels(const TextureDescription& description, uint32_t wanted)
        {
            const bool hasChain = description.levels > 1 || description.generateMips;
            uint32_t dropped = 0;

            while (hasChain && dropped < wanted
                && (std::max(description.width, description.height) >> (dropped + 1)) >= k_minDroppedSide
                && (description.generateMips || dropped + 1 < description.levels))
            {
                ++dropped;
            }

            return dropped;
        }

        float toLinear(std::byte value)
        {
            const float encoded = static_cast<float>(std::to_integer<uint32_t>(value)) / 255.0f;

            return encoded <= 0.04045f ? encoded / 12.92f : std::pow((encoded + 0.055f) / 1.055f, 2.4f);
        }

        std::byte toSrgbByte(float linear)
        {
            const float encoded = linear <= 0.0031308f ? linear * 12.92f
                : 1.055f * std::pow(linear, 1.0f / 2.4f) - 0.055f;

            return static_cast<std::byte>(std::clamp(std::lround(encoded * 255.0f), 0l, 255l));
        }

        std::vector<std::byte> halvedRgba(std::span<const std::byte> pixels, uint32_t width, uint32_t height,
            bool isSrgb)
        {
            const uint32_t halfWidth = std::max(1u, width / 2);
            const uint32_t halfHeight = std::max(1u, height / 2);
            std::vector<std::byte> halved(static_cast<std::size_t>(halfWidth) * halfHeight * k_rgbaChannels);

            for (uint32_t y = 0; y < halfHeight; ++y)
            {
                for (uint32_t x = 0; x < halfWidth; ++x)
                {
                    for (uint32_t channel = 0; channel < k_rgbaChannels; ++channel)
                    {
                        const bool decode = isSrgb && channel < 3;
                        float sum = 0.0f;

                        for (uint32_t tap = 0; tap < 4; ++tap)
                        {
                            const uint32_t sourceX = std::min(x * 2 + tap % 2, width - 1);
                            const uint32_t sourceY = std::min(y * 2 + tap / 2, height - 1);
                            const std::byte value = pixels[(static_cast<std::size_t>(sourceY) * width + sourceX)
                                * k_rgbaChannels + channel];
                            sum += decode ? toLinear(value) : static_cast<float>(std::to_integer<uint32_t>(value));
                        }

                        const float average = sum * 0.25f;
                        halved[(static_cast<std::size_t>(y) * halfWidth + x) * k_rgbaChannels + channel] = decode
                            ? toSrgbByte(average)
                            : static_cast<std::byte>(std::clamp(std::lround(average), 0l, 255l));
                    }
                }
            }

            return halved;
        }
    } //namespace

    bool Renderer::supportsBlockCompression() const
    {
        return m_state->context.getDeviceInfo().enabled(vela::core::Feature::TextureCompressionBC);
    }

    Result<TextureHandle> Renderer::createTexture(const TextureDescription& description)
    {
        if (description.width == 0 || description.height == 0 || description.levels == 0
            || description.levels > fullMipLevelCount(description.width, description.height))
        {
            return Error{ErrorCode::InvalidArgument, "createTexture: invalid size or mip level count"};
        }

        std::size_t expectedSize = 0;

        for (uint32_t level = 0; level < description.levels; ++level)
        {
            expectedSize += textureLevelBytes(description.format, std::max(1u, description.width >> level),
                std::max(1u, description.height >> level));
        }

        if (description.generateMips
            && (description.levels != 1 || (description.format != TextureFormat::Rgba8Srgb
                && description.format != TextureFormat::Rgba8Unorm)))
        {
            return Error{ErrorCode::InvalidArgument, "createTexture: generateMips needs one Rgba8 level"};
        }

        if (description.pixels.size() != expectedSize)
        {
            return Error{ErrorCode::InvalidArgument, "createTexture: expected " + std::to_string(expectedSize)
                + " bytes of pixel data, got " + std::to_string(description.pixels.size())};
        }

        const uint32_t dropped = droppableLevels(description, m_state->quality.textureMipDrop);
        std::span<const std::byte> pixels = description.pixels;
        std::vector<std::byte> downsampled;
        uint32_t width = description.width;
        uint32_t height = description.height;

        for (uint32_t level = 0; level < dropped; ++level)
        {
            if (description.generateMips)
            {
                downsampled = halvedRgba(pixels, width, height, description.format == TextureFormat::Rgba8Srgb);
                pixels = downsampled;
            }
            else
            {
                pixels = pixels.subspan(textureLevelBytes(description.format, width, height));
            }

            width = std::max(1u, width / 2);
            height = std::max(1u, height / 2);
        }

        vela::graphics::ImageData image{};
        image.pixels = pixels;
        image.width = width;
        image.height = height;
        image.format = toVelaFormat(description.format);
        image.levels = description.generateMips ? 1 : description.levels - dropped;

        const vela::graphics::TextureOptions options{.generateMips = description.generateMips};
        auto texture = vela::graphics::Texture::create(m_state->context, image, toVelaSampler(description.filter),
            options);

        if (!texture)
        {
            return toError(texture.error());
        }

        return m_state->textures.add(TextureRecord{.texture = std::move(texture).value(), .materialUses = 0});
    }
} //namespace lockstep::render
