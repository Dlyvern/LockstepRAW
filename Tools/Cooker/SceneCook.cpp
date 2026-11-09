#include "SceneCook.hpp"

#include <Lockstep/Assets/FileSystem.hpp>
#include <Lockstep/Core/Hash.hpp>
#include <Lockstep/Physics/MeshCooker.hpp>

#include "Assets/CookedFormat.hpp"
#include "Assets/MeshProcessing.hpp"

#include "CookerOutput.hpp"
#include "ParallelFor.hpp"
#include "SceneSource.hpp"
#include "TextureCook.hpp"

#include <array>
#include <chrono>
#include <cmath>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <map>
#include <optional>
#include <system_error>

namespace lockstep::cooker
{
    namespace
    {
        constexpr double k_bytesPerMegabyte{1024.0 * 1024.0};
        constexpr std::size_t k_usageCount{4};
        constexpr std::array<const char*, k_usageCount> k_usageNames{"colour (BC7 sRGB)", "normal (BC5)",
            "metallic-roughness (BC7)", "occlusion (BC4)"};

        using Clock = std::chrono::steady_clock;

        struct CookContext
        {
            const CookOptions& options;
            const assets::FileSystem& sourceFiles;
            const SourceScene& scene;
            std::string prefix;
            assets::CookedLevel level;
            uint64_t writtenBytes{0};
        };

        struct CollisionGeometry
        {
            std::vector<math::Vec3> positions;
            std::vector<uint32_t> indices;
        };

        double secondsSince(Clock::time_point start)
        {
            return std::chrono::duration<double>(Clock::now() - start).count();
        }

        std::string filePath(const CookContext& context, std::string_view folder, std::size_t index,
            std::string_view extension)
        {
            return context.prefix + "/" + std::string(folder) + "/" + std::to_string(index) + std::string(extension);
        }

        Status firstError(const std::vector<std::optional<Error>>& errors)
        {
            for (const std::optional<Error>& error : errors)
            {
                if (error)
                {
                    return *error;
                }
            }

            return {};
        }

        Status writeFile(CookContext& context, std::string_view path, std::span<const std::byte> bytes)
        {
            context.writtenBytes += bytes.size();
            return writeCookedFile(context.options.out, path, bytes);
        }

        Status cookMeshes(CookContext& context)
        {
            const std::vector<SourceMesh>& meshes = context.scene.meshes;
            std::vector<std::optional<Error>> errors(meshes.size());
            std::vector<uint64_t> sizes(meshes.size());
            context.level.meshes.resize(meshes.size());

            parallelFor(meshes.size(), context.options.threads, [&](std::size_t index)
            {
                const assets::PrimitiveGeometry& geometry = meshes[index].geometry;
                const std::string path = filePath(context, "meshes", index, ".mesh");
                const std::vector<std::byte> file = assets::writeCookedMesh(geometry.vertices, geometry.indices,
                    geometry.bounds);
                sizes[index] = file.size();

                if (auto written = writeCookedFile(context.options.out, path, file); !written)
                {
                    errors[index] = written.error();
                }

                assets::CookedMesh& cooked = context.level.meshes[index];
                cooked.path = path;
                cooked.material = meshes[index].material;
                cooked.alphaMode = meshes[index].alphaMode;
                cooked.localBounds = geometry.bounds;
                cooked.triangleCount = static_cast<uint32_t>(geometry.indices.size() / 3);
            });

            for (const uint64_t size : sizes)
            {
                context.writtenBytes += size;
            }

            return firstError(errors);
        }

        CollisionGeometry collisionGeometry(const assets::PrimitiveGeometry& geometry)
        {
            CollisionGeometry collision{};
            collision.positions.reserve(geometry.vertices.size());

            for (const render::PbrVertex& vertex : geometry.vertices)
            {
                collision.positions.emplace_back(vertex.position[0], vertex.position[1], vertex.position[2]);
            }

            collision.indices = assets::nonDegenerateTriangles(collision.positions, geometry.indices);

            return collision;
        }

        uint64_t geometryKey(const CollisionGeometry& geometry)
        {
            return hash64(std::as_bytes(std::span(geometry.indices)), hash64(std::as_bytes(std::span(geometry.positions))));
        }

        bool sameGeometry(const CollisionGeometry& left, const CollisionGeometry& right)
        {
            return left.indices == right.indices && left.positions.size() == right.positions.size()
                && std::memcmp(left.positions.data(), right.positions.data(),
                    left.positions.size() * sizeof(math::Vec3)) == 0;
        }

        std::vector<CollisionGeometry> uniqueShapes(CookContext& context)
        {
            std::vector<CollisionGeometry> shapes;
            std::multimap<uint64_t, uint32_t> shapeByKey;

            for (std::size_t index = 0; index < context.scene.meshes.size(); ++index)
            {
                CollisionGeometry geometry = collisionGeometry(context.scene.meshes[index].geometry);

                if (geometry.indices.empty())
                {
                    continue;
                }

                const uint64_t key = geometryKey(geometry);
                uint32_t shape = assets::k_noCollisionShape;

                for (auto [match, end] = shapeByKey.equal_range(key); match != end; ++match)
                {
                    if (sameGeometry(shapes[match->second], geometry))
                    {
                        shape = match->second;
                        break;
                    }
                }

                if (shape == assets::k_noCollisionShape)
                {
                    shape = static_cast<uint32_t>(shapes.size());
                    shapeByKey.emplace(key, shape);
                    shapes.push_back(std::move(geometry));
                }

                context.level.meshes[index].collisionShape = shape;
            }

            return shapes;
        }

        Status cookCollision(CookContext& context)
        {
            auto cooker = physics::MeshCooker::create();

            if (!cooker)
            {
                return cooker.error();
            }

            const std::vector<CollisionGeometry> shapes = uniqueShapes(context);
            std::vector<std::optional<Error>> errors(shapes.size());
            std::vector<std::vector<std::byte>> files(shapes.size());
            context.level.collisionShapes.resize(shapes.size());

            parallelFor(shapes.size(), context.options.threads, [&](std::size_t index)
            {
                auto cooked = cooker.value().cook(shapes[index].positions, shapes[index].indices);

                if (!cooked)
                {
                    errors[index] = cooked.error();
                    return;
                }

                files[index] = assets::writeCookedCollision(static_cast<uint32_t>(shapes[index].indices.size() / 3),
                    cooked.value());
                context.level.collisionShapes[index] = filePath(context, "collision", index, ".col");
            });

            if (auto failed = firstError(errors); !failed)
            {
                return failed;
            }

            for (std::size_t index = 0; index < files.size(); ++index)
            {
                if (auto written = writeFile(context, context.level.collisionShapes[index], files[index]); !written)
                {
                    return written;
                }
            }

            std::cout << "  collision: " << shapes.size() << " distinct shapes for " << context.scene.meshes.size()
                << " meshes\n";

            return {};
        }

        struct TextureOutcome
        {
            std::optional<CookedTexture> cooked;
            std::optional<Error> error;
        };

        void reportTextures(const CookContext& context, const std::vector<TextureOutcome>& outcomes)
        {
            std::array<double, k_usageCount> squaredError{};
            std::array<uint32_t, k_usageCount> counts{};
            uint64_t pixelBytes = 0;

            for (std::size_t index = 0; index < outcomes.size(); ++index)
            {
                if (!outcomes[index].cooked)
                {
                    continue;
                }

                const auto usage = static_cast<std::size_t>(context.scene.textures[index].usage);
                squaredError[usage] += outcomes[index].cooked->meanSquaredError;
                ++counts[usage];
                pixelBytes += outcomes[index].cooked->pixelBytes;
            }

            std::cout << "  textures: " << context.level.textures.size() << " cooked, "
                << static_cast<double>(pixelBytes) / k_bytesPerMegabyte << " MiB of GPU data with mips\n";

            for (std::size_t usage = 0; usage < k_usageCount; ++usage)
            {
                if (counts[usage] == 0)
                {
                    continue;
                }

                const double meanError = squaredError[usage] / counts[usage];
                const double psnr = meanError > 0.0 ? 10.0 * std::log10(255.0 * 255.0 / meanError) : 99.0;
                std::cout << "    " << k_usageNames[usage] << ": " << counts[usage] << ", mean base-level PSNR "
                    << psnr << " dB\n";
            }
        }

        uint32_t remapTexture(const std::vector<uint32_t>& remap, uint32_t index)
        {
            return index == assets::k_noTexture ? index : remap[index];
        }

        Status cookTextures(CookContext& context)
        {
            const std::vector<SourceTexture>& textures = context.scene.textures;
            std::vector<TextureOutcome> outcomes(textures.size());
            assets::TranscoderGate gate{};

            parallelFor(textures.size(), context.options.threads, [&](std::size_t index)
            {
                auto bytes = assets::readImageBytes(*textures[index].image, context.sourceFiles,
                    context.scene.directory);
                auto cooked = bytes ? cookTexture(bytes.value(), textures[index].usage, gate)
                    : Result<CookedTexture>(bytes.error());

                if (!cooked)
                {
                    outcomes[index].error = cooked.error();
                    return;
                }

                outcomes[index].cooked = std::move(cooked).value();
            });

            std::vector<uint32_t> remap(textures.size(), assets::k_noTexture);

            for (std::size_t index = 0; index < outcomes.size(); ++index)
            {
                if (!outcomes[index].cooked)
                {
                    const cgltf_image& image = *textures[index].image;
                    std::cout << "  warning: image '" << (image.uri != nullptr ? image.uri : "embedded")
                        << "' skipped: " << outcomes[index].error->message << '\n';
                    continue;
                }

                remap[index] = static_cast<uint32_t>(context.level.textures.size());
                const std::string path = filePath(context, "textures", context.level.textures.size(), ".tex");
                context.level.textures.push_back(path);

                if (auto written = writeFile(context, path, outcomes[index].cooked->file); !written)
                {
                    return written;
                }
            }

            for (assets::CookedMaterial& material : context.level.materials)
            {
                material.baseColor = remapTexture(remap, material.baseColor);
                material.normal = remapTexture(remap, material.normal);
                material.metallicRoughness = remapTexture(remap, material.metallicRoughness);
                material.occlusion = remapTexture(remap, material.occlusion);
                material.emissive = remapTexture(remap, material.emissive);
            }

            reportTextures(context, outcomes);

            return {};
        }

        Status prepareOutput(const CookOptions& options, std::string& prefix)
        {
            auto normalized = assets::normalizePath(options.name);

            if (!normalized)
            {
                return normalized.error();
            }

            prefix = std::move(normalized).value();
            std::error_code error;
            std::filesystem::remove_all(options.out / std::filesystem::path(prefix), error);

            if (error)
            {
                return Error{ErrorCode::AssetLoadFailed, "cannot clear '" + (options.out / prefix).string() + "'"};
            }

            return {};
        }
    } //namespace

    Status cookScene(const CookOptions& options)
    {
        const Clock::time_point start = Clock::now();
        std::string prefix;

        if (auto prepared = prepareOutput(options, prefix); !prepared)
        {
            return prepared;
        }

        assets::FileSystem sourceFiles;
        const std::filesystem::path folder = options.source.has_parent_path() ? options.source.parent_path() : ".";

        if (auto mounted = sourceFiles.mountDirectory(folder); !mounted)
        {
            return mounted;
        }

        auto scene = readSourceScene(sourceFiles, options.source.filename().string(), options.scale);

        if (!scene)
        {
            return scene.error();
        }

        std::cout << std::fixed << std::setprecision(2) << "LockstepCooker: '" << options.source.string() << "' -> '"
            << prefix << "/scene': " << scene.value().meshes.size() << " meshes, " << scene.value().instances.size()
            << " instances, " << scene.value().materials.size() << " materials, " << scene.value().textures.size()
            << " textures (read in " << secondsSince(start) << " s)\n";

        CookContext context{.options = options, .sourceFiles = sourceFiles, .scene = scene.value(), .prefix = prefix,
            .level = {}, .writtenBytes = 0};
        context.level.materials = scene.value().materials;
        context.level.instances = scene.value().instances;
        context.level.nodes = scene.value().nodes;
        context.level.lights = scene.value().lights;
        context.level.bounds = scene.value().bounds;

        if (auto cooked = cookMeshes(context); !cooked)
        {
            return cooked;
        }

        if (auto cooked = cookCollision(context); !cooked)
        {
            return cooked;
        }

        Clock::time_point textureStart = Clock::now();

        if (auto cooked = cookTextures(context); !cooked)
        {
            return cooked;
        }

        std::cout << "  texture encoding: " << secondsSince(textureStart) << " s\n";

        if (auto written = writeFile(context, prefix + "/scene", assets::writeCookedLevel(context.level)); !written)
        {
            return written;
        }

        for (const std::string& warning : scene.value().warnings)
        {
            std::cout << "  import: " << warning << '\n';
        }

        std::cout << "  " << context.level.nodes.size() << " named nodes, " << context.level.lights.size()
            << " lights; wrote " << static_cast<double>(context.writtenBytes) / k_bytesPerMegabyte << " MiB in "
            << secondsSince(start) << " s\n" << std::defaultfloat;

        return {};
    }
} //namespace lockstep::cooker
