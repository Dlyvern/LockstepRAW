#include "TextureImport.hpp"

#include "Lockstep/Render/Renderer.hpp"

#include "GltfDocument.hpp"
#include "ImageDecode.hpp"

#include <algorithm>
#include <condition_variable>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

namespace lockstep::assets
{
    namespace
    {
        constexpr std::size_t k_decodedPerWorker{2};

        struct DecodeQueue
        {
            std::mutex mutex;
            std::condition_variable changed;
            std::size_t nextJob{0};
            std::size_t uploaded{0};
            std::size_t maxAhead{1};
            std::vector<std::optional<Result<DecodedTexture>>> results;
        };

        Result<DecodedTexture> decodeKey(const TextureKey& key, const std::filesystem::path& directory, const DecodeTargets& targets,
            TranscoderGate& gate)
        {
            auto bytes = readImageBytes(*key.image, directory);

            if (!bytes)
            {
                return bytes.error();
            }

            return decodeImage(bytes.value(), key.srgb, targets, gate);
        }

        void decodeWorker(DecodeQueue& queue, const std::vector<TextureKey>& keys,
            const std::filesystem::path& directory, const DecodeTargets& targets, TranscoderGate& gate)
        {
            while (true)
            {
                std::size_t job = 0;

                {
                    std::unique_lock lock(queue.mutex);
                    queue.changed.wait(lock, [&queue, &keys]
                    {
                        return queue.nextJob >= keys.size() || queue.nextJob < queue.uploaded + queue.maxAhead;
                    });

                    if (queue.nextJob >= keys.size())
                    {
                        return;
                    }

                    job = queue.nextJob++;
                }

                Result<DecodedTexture> decoded = decodeKey(keys[job], directory, targets, gate);

                {
                    std::lock_guard lock(queue.mutex);
                    queue.results[job].emplace(std::move(decoded));
                }

                queue.changed.notify_all();
            }
        }

        Result<DecodedTexture> takeResult(DecodeQueue& queue, std::size_t job)
        {
            std::unique_lock lock(queue.mutex);
            queue.changed.wait(lock, [&queue, job] { return queue.results[job].has_value(); });
            Result<DecodedTexture> result = std::move(*queue.results[job]);
            queue.results[job].reset();
            queue.uploaded = job + 1;
            lock.unlock();
            queue.changed.notify_all();

            return result;
        }

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

        uint32_t workerCount(const SceneImportOptions& options, std::size_t jobs)
        {
            const uint32_t requested = options.workerThreads != 0
                ? options.workerThreads
                : std::max(1u, std::thread::hardware_concurrency());

            return static_cast<uint32_t>(std::min<std::size_t>(requested, std::max<std::size_t>(jobs, 1)));
        }
    } //namespace

    Status importTextures(render::Renderer& renderer, TextureTable& textures, const std::filesystem::path& directory,
        const SceneImportOptions& options, SceneStats& stats, ImportWarnings& warnings)
    {
        const std::vector<TextureKey>& keys = textures.keys();

        if (keys.empty())
        {
            return {};
        }

        const uint32_t workers = workerCount(options, keys.size());
        DecodeQueue queue{};
        queue.results.resize(keys.size());
        queue.maxAhead = workers * k_decodedPerWorker;
        TranscoderGate gate{};
        const DecodeTargets targets{
            .blockCompression = renderer.supportsBlockCompression(),
            .compress = options.compressTextures};

        std::vector<std::jthread> threads;
        threads.reserve(workers);

        for (uint32_t worker = 0; worker < workers; ++worker)
        {
            threads.emplace_back([&queue, &keys, &directory, &targets, &gate]
            {
                decodeWorker(queue, keys, directory, targets, gate);
            });
        }

        std::optional<Error> uploadError;

        for (std::size_t slot = 0; slot < keys.size(); ++slot)
        {
            Result<DecodedTexture> decoded = takeResult(queue, slot);

            if (uploadError)
            {
                continue;
            }

            if (!decoded)
            {
                warnings.add("image '" + imageName(*keys[slot].image, slot) + "' skipped: " + decoded.error().message);
                continue;
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
                continue;
            }

            textures.setHandle(slot, handle.value());
            ++stats.textureCount;
            stats.textureBytes += gpuBytes(texture);
            stats.compressedTextures += isBlockCompressed(texture.format) ? 1u : 0u;
        }

        threads.clear();

        if (uploadError)
        {
            return *uploadError;
        }

        return {};
    }
} //namespace lockstep::assets
