#include "ImageDecode.hpp"

#include "MipChain.hpp"

#include <ktx.h>
#include <stb_image.h>

#include <algorithm>
#include <array>
#include <cstring>
#include <limits>
#include <memory>
#include <optional>
#include <string>

namespace lockstep::assets
{
    namespace
    {
        constexpr std::array<uint8_t, 12> k_ktx2Signature{
            0xAB, 0x4B, 0x54, 0x58, 0x20, 0x32, 0x30, 0xBB, 0x0D, 0x0A, 0x1A, 0x0A};

        constexpr uint32_t k_vkR8G8B8A8Unorm{37};
        constexpr uint32_t k_vkR8G8B8A8Srgb{43};
        constexpr uint32_t k_vkBc1RgbUnorm{131};
        constexpr uint32_t k_vkBc1RgbSrgb{132};
        constexpr uint32_t k_vkBc1RgbaUnorm{133};
        constexpr uint32_t k_vkBc1RgbaSrgb{134};
        constexpr uint32_t k_vkBc3Unorm{137};
        constexpr uint32_t k_vkBc3Srgb{138};
        constexpr uint32_t k_vkBc7Unorm{145};
        constexpr uint32_t k_vkBc7Srgb{146};

        struct KtxDeleter
        {
            void operator()(ktxTexture2* texture) const noexcept
            {
                ktxTexture2_Destroy(texture);
            }
        };

        using KtxPointer = std::unique_ptr<ktxTexture2, KtxDeleter>;

        struct StbDeleter
        {
            void operator()(stbi_uc* pixels) const noexcept
            {
                stbi_image_free(pixels);
            }
        };

        Error decodeError(std::string message)
        {
            return Error{ErrorCode::AssetLoadFailed, std::move(message)};
        }

        bool isKtx2(std::span<const std::byte> encoded)
        {
            return encoded.size() >= k_ktx2Signature.size()
                && std::memcmp(encoded.data(), k_ktx2Signature.data(), k_ktx2Signature.size()) == 0;
        }

        std::optional<render::TextureFormat> storedFormat(uint32_t vkFormat, bool srgb)
        {
            switch (vkFormat)
            {
                case k_vkR8G8B8A8Unorm:
                case k_vkR8G8B8A8Srgb:
                {
                    return srgb ? render::TextureFormat::Rgba8Srgb : render::TextureFormat::Rgba8Unorm;
                }
                case k_vkBc1RgbUnorm:
                case k_vkBc1RgbSrgb:
                case k_vkBc1RgbaUnorm:
                case k_vkBc1RgbaSrgb:
                {
                    return srgb ? render::TextureFormat::Bc1Srgb : render::TextureFormat::Bc1Unorm;
                }
                case k_vkBc3Unorm:
                case k_vkBc3Srgb:
                {
                    return srgb ? render::TextureFormat::Bc3Srgb : render::TextureFormat::Bc3Unorm;
                }
                case k_vkBc7Unorm:
                case k_vkBc7Srgb:
                {
                    return srgb ? render::TextureFormat::Bc7Srgb : render::TextureFormat::Bc7Unorm;
                }
                default:
                {
                    return std::nullopt;
                }
            }
        }

        DecodedTexture copyLevels(ktxTexture2* texture, render::TextureFormat format)
        {
            DecodedTexture decoded{};
            decoded.format = format;
            decoded.width = texture->baseWidth;
            decoded.height = texture->baseHeight;
            decoded.levels = std::min(texture->numLevels, render::fullMipLevelCount(decoded.width, decoded.height));

            for (uint32_t level = 0; level < decoded.levels; ++level)
            {
                ktx_size_t offset = 0;
                ktxTexture_GetImageOffset(ktxTexture(texture), level, 0, 0, &offset);
                const std::size_t size = render::textureLevelBytes(format, std::max(1u, decoded.width >> level),
                    std::max(1u, decoded.height >> level));
                const auto* source = reinterpret_cast<const std::byte*>(texture->pData + offset);
                decoded.bytes.insert(decoded.bytes.end(), source, source + size);
            }

            return decoded;
        }

        DecodedTexture mipmapRgba8Level(ktxTexture2* texture, bool srgb, bool compress)
        {
            ktx_size_t offset = 0;
            ktxTexture_GetImageOffset(ktxTexture(texture), 0, 0, 0, &offset);
            const std::size_t size = static_cast<std::size_t>(texture->baseWidth) * texture->baseHeight * 4;

            return buildMippedTexture(std::span<const uint8_t>(texture->pData + offset, size), texture->baseWidth,
                texture->baseHeight, srgb, compress);
        }

        ktx_error_code_e transcode(ktxTexture2* texture, ktx_transcode_fmt_e target, TranscoderGate& gate)
        {
            if (gate.isInitialized.load(std::memory_order_acquire))
            {
                return ktxTexture2_TranscodeBasis(texture, target, 0);
            }

            std::lock_guard lock(gate.mutex);
            const ktx_error_code_e result = ktxTexture2_TranscodeBasis(texture, target, 0);
            gate.isInitialized.store(true, std::memory_order_release);

            return result;
        }

        Result<DecodedTexture> decodeKtx2(std::span<const std::byte> encoded, bool srgb, bool compress,
            TranscoderGate& gate)
        {
            ktxTexture2* raw = nullptr;
            const ktx_error_code_e created = ktxTexture2_CreateFromMemory(
                reinterpret_cast<const ktx_uint8_t*>(encoded.data()), encoded.size(),
                KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT, &raw);

            if (created != KTX_SUCCESS)
            {
                return decodeError(std::string("KTX2: ") + ktxErrorString(created));
            }

            const KtxPointer texture(raw);

            if (texture->numDimensions != 2 || texture->isArray || texture->isCubemap || texture->numFaces != 1
                || texture->baseWidth == 0 || texture->baseHeight == 0)
            {
                return decodeError("KTX2: only single 2D images are supported");
            }

            if (ktxTexture2_NeedsTranscoding(texture.get()))
            {
                const bool hasMips = texture->numLevels > 1;
                const ktx_error_code_e transcoded = transcode(texture.get(), hasMips ? KTX_TTF_BC7_RGBA : KTX_TTF_RGBA32,
                    gate);

                if (transcoded != KTX_SUCCESS)
                {
                    return decodeError(std::string("KTX2 transcode: ") + ktxErrorString(transcoded));
                }

                if (hasMips)
                {
                    return copyLevels(texture.get(), srgb ? render::TextureFormat::Bc7Srgb : render::TextureFormat::Bc7Unorm);
                }

                return mipmapRgba8Level(texture.get(), srgb, compress);
            }

            const std::optional<render::TextureFormat> format = storedFormat(texture->vkFormat, srgb);

            if (!format)
            {
                return decodeError("KTX2: unsupported vkFormat " + std::to_string(texture->vkFormat));
            }

            if (!isBlockCompressed(*format) && texture->numLevels == 1)
            {
                return mipmapRgba8Level(texture.get(), srgb, compress);
            }

            return copyLevels(texture.get(), *format);
        }

        Result<DecodedTexture> decodeStb(std::span<const std::byte> encoded, bool srgb, bool compress)
        {
            if (encoded.size() > static_cast<std::size_t>(std::numeric_limits<int>::max()))
            {
                return decodeError("image too large");
            }

            int width = 0;
            int height = 0;
            int channels = 0;
            const std::unique_ptr<stbi_uc, StbDeleter> pixels(stbi_load_from_memory(
                reinterpret_cast<const stbi_uc*>(encoded.data()), static_cast<int>(encoded.size()), &width, &height,
                &channels, STBI_rgb_alpha));

            if (pixels == nullptr || width <= 0 || height <= 0)
            {
                const char* reason = stbi_failure_reason();
                return decodeError(std::string("image decode: ") + (reason != nullptr ? reason : "unknown format"));
            }

            const std::size_t size = static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4;

            return buildMippedTexture(std::span<const uint8_t>(pixels.get(), size), static_cast<uint32_t>(width),
                static_cast<uint32_t>(height), srgb, compress);
        }
    } //namespace

    bool isBlockCompressed(render::TextureFormat format) noexcept
    {
        return format != render::TextureFormat::Rgba8Srgb && format != render::TextureFormat::Rgba8Unorm;
    }

    Result<DecodedTexture> decodeImage(std::span<const std::byte> encoded, bool srgb, bool compress,
        TranscoderGate& gate)
    {
        if (isKtx2(encoded))
        {
            return decodeKtx2(encoded, srgb, compress, gate);
        }

        return decodeStb(encoded, srgb, compress);
    }
} //namespace lockstep::assets
