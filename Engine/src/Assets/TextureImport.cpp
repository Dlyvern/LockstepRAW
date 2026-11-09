#include "TextureImport.hpp"

#include "Lockstep/Render/Renderer.hpp"

#include "GltfDocument.hpp"
#include "ImageDecode.hpp"
#include "OrderedWork.hpp"

#include <algorithm>
#include <optional>
#include <string>
#include <vector>

namespace lockstep::assets
{
    namespace
    {
        constexpr std::size_t k_decodedPerWorker{2};

        std::string imageName(const cgltf_image& image, std::size_t slot)
        {
            if (image.uri != nullptr && std::string_view(image.uri).find("data:") != 0)
            {
                return image.uri;
            }

            if (image.name != nullptr)
            {
                return image.name;
            }

            return "#" + std::to_string(slot);
        }

        uint64_t gpuBytes(const DecodedTexture& texture)
        {
            if (!texture.generateMips)
            {
                return texture.bytes.size();
            }

            uint64_t total = 0;

            for (uint32_t level = 0; level < render::fullMipLevelCount(texture.width, texture.height); ++level)
            {
                total += render::textureLevelBytes(texture.format, std::max(1u, texture.width >> level),
                    std::max(1u, texture.height >> level));
            }

            return total;
        }
    } //namespace

    Status importTextures(render::Renderer& renderer, TextureTable& textures, const FileSystem& files,
        std::string_view directory, const SceneImportOptions& options, SceneStats& stats, ImportWarnings& warnings)
    {
        const std::vector<TextureKey>& keys = textures.keys();
        const uint32_t workers = workerCountFor(options.workerThreads, keys.size());
        TranscoderGate gate{};
        const DecodeTargets targets{
            .blockCompression = renderer.supportsBlockCompression(),
            .compress = options.compressTextures};
        std::optional<Error> uploadError;

        const auto decode = [&keys, &files, directory, &targets, &gate](std::size_t slot) -> Result<DecodedTexture>
        {
            auto bytes = readImageBytes(*keys[slot].image, files, directory);

            if (!bytes)
            {
                return bytes.error();
            }

            return decodeImage(bytes.value(), keys[slot].srgb, targets, gate);
        };

        const auto upload = [&](std::size_t slot, Result<DecodedTexture>&& decoded)
        {
            if (uploadError)
            {
                return;
            }

            if (!decoded)
            {
                warnings.add("image '" + imageName(*keys[slot].image, slot) + "' skipped: " + decoded.error().message);
                return;
            }

            const DecodedTexture& texture = decoded.value();
            auto handle = renderer.createTexture(render::TextureDescription{
                .pixels = texture.bytes,
                .width = texture.width,
                .height = texture.height,
                .format = texture.format,
                .filter = render::TextureFilter::Linear,
                .levels = texture.levels,
                .generateMips = texture.generateMips});

            if (!handle)
            {
                uploadError = handle.error();
                return;
            }

            textures.setHandle(slot, handle.value());
            ++stats.textureCount;
            stats.textureBytes += gpuBytes(texture);
            stats.compressedTextures += isBlockCompressed(texture.format) ? 1u : 0u;
        };

        runOrdered<Result<DecodedTexture>>(keys.size(), workers, workers * k_decodedPerWorker, decode, upload);

        if (uploadError)
        {
            return *uploadError;
        }

        return {};
    }
} //namespace lockstep::assets
