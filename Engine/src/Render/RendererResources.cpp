#include "Lockstep/Render/Renderer.hpp"

#include "RendererState.hpp"
#include "VelaConversions.hpp"

#include <Vela/Graphics/ImageData.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>

namespace lockstep::render
{
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

        vela::graphics::ImageData image{};
        image.pixels = description.pixels;
        image.width = description.width;
        image.height = description.height;
        image.format = toVelaFormat(description.format);
        image.levels = description.levels;

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
