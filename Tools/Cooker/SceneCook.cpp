#include "SceneCook.hpp"

#include <Lockstep/Assets/FileSystem.hpp>
#include <Lockstep/Core/Hash.hpp>
#include <Lockstep/Physics/MeshCooker.hpp>

#include "Assets/CookedFormat.hpp"
#include "Assets/CookedIrradiance.hpp"
#include "Assets/MeshProcessing.hpp"

#include "CookerOutput.hpp"
#include "LimbSplit.hpp"
#include "ParallelFor.hpp"
#include "SceneSource.hpp"
#include "TextureCook.hpp"

#include <array>
#include <chrono>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <optional>
#include <string>
#include <system_error>

namespace lockstep::cooker
{
    namespace
    {
        constexpr double k_bytesPerMegabyte{1024.0 * 1024.0};
        constexpr std::size_t k_usageCount{5};
        constexpr std::array<const char*, k_usageCount> k_usageNames{"colour (BC7 sRGB)", "normal (BC5)",
            "metallic-roughness (BC7)", "occlusion (BC4)", "height (BC4)"};

        constexpr math::Vec4 k_woundColour{0.32f, 0.018f, 0.014f, 1.0f};
        constexpr float k_woundRoughness{0.32f};
        constexpr std::size_t k_maskedParts{64};
        constexpr uint32_t k_noMaterial{0xFFFFFFFFu};
        constexpr std::string_view k_woundNode{"Wound_Stump"};
        constexpr float k_woundFaceUp{0.9f};

        using Clock = std::chrono::steady_clock;

        struct CookContext
        {
            const CookOptions& options;
            const assets::FileSystem& sourceFiles;
            const SourceScene& scene;
            std::string prefix;
            assets::CookedLevel level;
            uint64_t writtenBytes{0};
            uint32_t woundMaterial{k_noMaterial};
            std::vector<anim::LimbInfo> limbs{};
            const SourceScene* woundScene{nullptr};
            const assets::FileSystem* woundFiles{nullptr};
            std::optional<WoundTemplate> woundTemplate{};
            uint32_t woundMesh{0};
            std::vector<uint32_t> woundTextures{};
            std::vector<TextureMean> textureMeans{};
        };

        struct TextureSource
        {
            const SourceTexture* texture{nullptr};
            const assets::FileSystem* files{nullptr};
            const std::string* directory{nullptr};
            uint32_t cap{0};
        };

        uint32_t takeWoundTexture(CookContext& context, uint32_t index)
        {
            if (index == assets::k_noTexture)
            {
                return index;
            }

            context.woundTextures.push_back(index);
            return static_cast<uint32_t>(context.scene.textures.size() + context.woundTextures.size() - 1);
        }

        std::optional<WoundTemplate> woundTemplateOf(const SourceScene& scene, uint32_t& meshIndex)
        {
            for (const assets::SceneInstance& instance : scene.instances)
            {
                if (instance.node >= scene.nodes.size() || scene.nodes[instance.node].name != k_woundNode)
                {
                    continue;
                }

                const assets::PrimitiveGeometry& geometry = scene.meshes[instance.mesh].geometry;
                const math::Mat4 placement = math::toMatrix(instance.transform);
                WoundTemplate wound{.vertices = geometry.vertices, .indices = geometry.indices};
                math::Aabb bounds{};

                for (render::PbrVertex& vertex : wound.vertices)
                {
                    const math::Vec3 position = math::transformPoint(placement,
                        math::Vec3{vertex.position[0], vertex.position[1], vertex.position[2]});
                    const math::Vec3 normal = math::normalize(math::transformDirection(placement,
                        math::Vec3{vertex.normal[0], vertex.normal[1], vertex.normal[2]}));
                    const math::Vec3 tangent = math::normalize(math::transformDirection(placement,
                        math::Vec3{vertex.tangent[0], vertex.tangent[1], vertex.tangent[2]}));
                    vertex.position[0] = position.x;
                    vertex.position[1] = position.y;
                    vertex.position[2] = position.z;
                    vertex.normal[0] = normal.x;
                    vertex.normal[1] = normal.y;
                    vertex.normal[2] = normal.z;
                    vertex.tangent[0] = tangent.x;
                    vertex.tangent[1] = tangent.y;
                    vertex.tangent[2] = tangent.z;
                    math::expand(bounds, position);
                }

                const float radius = 0.5f * std::max(bounds.max.x - bounds.min.x, bounds.max.z - bounds.min.z);

                if (!(radius > 0.0f))
                {
                    return std::nullopt;
                }

                float faceArea = 0.0f;
                float faceHeight = 0.0f;

                for (std::size_t first = 0; first + 2 < wound.indices.size(); first += 3)
                {
                    std::array<math::Vec3, 3> corners{};

                    for (std::size_t corner = 0; corner < 3; ++corner)
                    {
                        const render::PbrVertex& vertex = wound.vertices[wound.indices[first + corner]];
                        corners[corner] = math::Vec3{vertex.position[0], vertex.position[1], vertex.position[2]};
                    }

                    const math::Vec3 normal = math::cross(corners[1] - corners[0], corners[2] - corners[0]);
                    const float area = math::length(normal);

                    if (area > 0.0f && normal.y / area > k_woundFaceUp)
                    {
                        faceArea += area;
                        faceHeight += area * (corners[0].y + corners[1].y + corners[2].y) / 3.0f;
                    }
                }

                const std::array<float, 3> face{0.5f * (bounds.min.x + bounds.max.x),
                    faceArea > 0.0f ? faceHeight / faceArea : bounds.max.y, 0.5f * (bounds.min.z + bounds.max.z)};

                for (render::PbrVertex& vertex : wound.vertices)
                {
                    for (std::size_t axis = 0; axis < 3; ++axis)
                    {
                        vertex.position[axis] = (vertex.position[axis] - face[axis]) / radius;
                    }
                }

                meshIndex = instance.mesh;
                return wound;
            }

            return std::nullopt;
        }

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

        uint32_t woundMaterial(CookContext& context)
        {
            if (context.woundMaterial < context.level.materials.size())
            {
                return context.woundMaterial;
            }

            assets::CookedMaterial wound{};
            wound.factors.baseColorFactor = k_woundColour;
            wound.factors.metallicFactor = 0.0f;
            wound.factors.roughnessFactor = k_woundRoughness;

            if (context.woundScene != nullptr && context.woundTemplate)
            {
                wound = context.woundScene->materials[context.woundScene->meshes[context.woundMesh].material];
                wound.baseColor = takeWoundTexture(context, wound.baseColor);
                wound.normal = takeWoundTexture(context, wound.normal);
                wound.metallicRoughness = takeWoundTexture(context, wound.metallicRoughness);
                wound.occlusion = takeWoundTexture(context, wound.occlusion);
                wound.emissive = takeWoundTexture(context, wound.emissive);
            }
            context.woundMaterial = static_cast<uint32_t>(context.level.materials.size());
            context.level.materials.push_back(wound);

            return context.woundMaterial;
        }

        std::vector<LimbSplitPart> skinnedParts(CookContext& context, const SourceSkinnedModel& source)
        {
            const assets::GltfSkinnedModel& model = source.model;
            std::vector<std::string> names;

            for (const assets::SkinnedPart& part : model.parts)
            {
                names.emplace_back(part.material != nullptr && part.material->name != nullptr ? part.material->name : "");
            }

            if (context.options.splitLimbs)
            {
                auto split = splitLimbs(model, source.materials, names, woundMaterial(context),
                    context.woundTemplate ? &*context.woundTemplate : nullptr);

                if (split)
                {
                    std::cout << "  limbs of '" << model.name << "': " << split.value().parts.size() << " parts, "
                        << split.value().capLoops << " cut loops\n";

                    if (split.value().parts.size() > k_maskedParts)
                    {
                        std::cout << "  warning: '" << model.name << "' has more than " << k_maskedParts
                            << " parts; the rest can't be hidden\n";
                    }

                    context.limbs = std::move(split.value().limbs);
                    return std::move(split.value().parts);
                }

                std::cout << "  limbs of '" << model.name << "' not split: " << split.error().message << '\n';
            }

            context.limbs.clear();
            std::vector<LimbSplitPart> parts;

            for (std::size_t part = 0; part < model.parts.size(); ++part)
            {
                parts.push_back(LimbSplitPart{.geometry = model.parts[part].geometry, .material = source.materials[part],
                    .materialName = names[part], .limb = anim::Limb::None, .role = anim::LimbPartRole::Whole});
            }

            return parts;
        }

        Status cookSkinnedModels(CookContext& context)
        {
            uint32_t meshIndex = 0;

            for (std::size_t index = 0; index < context.scene.skinnedModels.size(); ++index)
            {
                const SourceSkinnedModel& source = context.scene.skinnedModels[index];
                const assets::GltfSkinnedModel& model = source.model;
                assets::CookedModel cooked{.name = model.name, .skeleton = model.skeleton, .clips = model.clips,
                    .jointRadii = model.jointRadii, .bindBounds = model.bindBounds, .parts = {}, .limbs = {}};
                const std::vector<LimbSplitPart> parts = skinnedParts(context, source);
                cooked.limbs = std::move(context.limbs);

                for (const LimbSplitPart& part : parts)
                {
                    const assets::SkinnedGeometry& geometry = part.geometry;
                    const std::string path = filePath(context, "skinned", meshIndex++, ".mesh");

                    if (auto written = writeFile(context, path, assets::writeCookedSkinnedMesh(geometry.vertices,
                            geometry.indices, geometry.bounds)); !written)
                    {
                        return written;
                    }

                    cooked.parts.push_back(assets::CookedSkinnedPart{.path = path, .material = part.material,
                        .materialName = part.materialName, .limb = part.limb, .role = part.role});
                }

                const std::string path = filePath(context, "models", index, ".model");

                if (auto written = writeFile(context, path, assets::writeCookedModel(cooked)); !written)
                {
                    return written;
                }

                context.level.skinnedInstances.push_back(assets::SceneSkinnedInstance{
                    .model = static_cast<uint32_t>(context.level.models.size()), .transform = model.transform,
                    .node = model.node});
                context.level.models.push_back(path);
                std::cout << "  skinned model '" << model.name << "': " << model.skeleton.joints.size() << " joints, "
                    << cooked.parts.size() << " parts, " << model.clips.size() << " clips\n";
            }

            return {};
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

        void reportTextures(const CookContext& context, const std::vector<TextureSource>& textures,
            const std::vector<TextureOutcome>& outcomes)
        {
            std::array<double, k_usageCount> squaredError{};
            std::array<uint32_t, k_usageCount> counts{};
            uint64_t pixelBytes = 0;
            uint64_t uncappedBytes = 0;
            uint32_t cappedCount = 0;

            for (std::size_t index = 0; index < outcomes.size(); ++index)
            {
                if (!outcomes[index].cooked)
                {
                    continue;
                }

                const CookedTexture& cooked = *outcomes[index].cooked;
                const auto usage = static_cast<std::size_t>(textures[index].texture->usage);
                squaredError[usage] += cooked.meanSquaredError;
                ++counts[usage];
                pixelBytes += cooked.pixelBytes;
                uncappedBytes += cooked.uncappedBytes;
                cappedCount += cooked.width != cooked.sourceWidth || cooked.height != cooked.sourceHeight ? 1 : 0;
            }

            const TextureCapOptions& caps = context.options.textureCaps;
            std::cout << "  textures: " << context.level.textures.size() << " cooked, "
                << static_cast<double>(pixelBytes) / k_bytesPerMegabyte << " MiB of GPU data with mips ("
                << static_cast<double>(uncappedBytes) / k_bytesPerMegabyte << " MiB uncapped); " << cappedCount
                << " capped (max " << caps.maxSize << ", auto "
                << (caps.automatic ? std::to_string(static_cast<uint32_t>(caps.texelsPerMetre)) + " texels/m" : "off")
                << ")\n";

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

        std::string sourceName(const SourceTexture& texture)
        {
            if (texture.image == nullptr)
            {
                return texture.file.string();
            }

            return texture.image->uri != nullptr ? texture.image->uri : "embedded";
        }

        Result<std::vector<std::byte>> readSourceFile(const std::filesystem::path& path)
        {
            std::ifstream file(path, std::ios::binary | std::ios::ate);

            if (!file)
            {
                return Error{ErrorCode::AssetLoadFailed, "cannot open '" + path.string() + "'"};
            }

            std::vector<std::byte> bytes(static_cast<std::size_t>(file.tellg()));
            file.seekg(0);

            if (!file.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size())))
            {
                return Error{ErrorCode::AssetLoadFailed, "cannot read '" + path.string() + "'"};
            }

            return bytes;
        }

        uint32_t remapTexture(const std::vector<uint32_t>& remap, uint32_t index)
        {
            return index == assets::k_noTexture ? index : remap[index];
        }

        Status cookTextures(CookContext& context)
        {
            const std::vector<uint32_t> caps = textureCaps(context.scene, context.options.textureCaps);
            std::vector<TextureSource> textures;

            for (std::size_t index = 0; index < context.scene.textures.size(); ++index)
            {
                textures.push_back(TextureSource{.texture = &context.scene.textures[index],
                    .files = &context.sourceFiles, .directory = &context.scene.directory, .cap = caps[index]});
            }

            if (context.woundScene != nullptr)
            {
                for (const uint32_t index : context.woundTextures)
                {
                    textures.push_back(TextureSource{.texture = &context.woundScene->textures[index],
                        .files = context.woundFiles,
                        .directory = &context.woundScene->directory, .cap = context.options.textureCaps.maxSize});
                }
            }

            std::vector<TextureOutcome> outcomes(textures.size());
            assets::TranscoderGate gate{};

            parallelFor(textures.size(), context.options.threads, [&](std::size_t index)
            {
                const SourceTexture& texture = *textures[index].texture;
                auto bytes = texture.image != nullptr
                    ? assets::readImageBytes(*texture.image, *textures[index].files, *textures[index].directory)
                    : readSourceFile(texture.file);
                auto cooked = bytes ? cookTexture(bytes.value(), texture.usage, textures[index].cap, texture.alphaCutoff,
                    gate)
                    : Result<CookedTexture>(bytes.error());

                if (!cooked)
                {
                    outcomes[index].error = cooked.error();
                    return;
                }

                outcomes[index].cooked = std::move(cooked).value();
            });

            std::vector<uint32_t> remap(textures.size(), assets::k_noTexture);
            context.textureMeans.assign(context.scene.textures.size(), TextureMean{});

            for (std::size_t index = 0; index < outcomes.size(); ++index)
            {
                if (!outcomes[index].cooked)
                {
                    std::cout << "  warning: image '" << sourceName(*textures[index].texture) << "' skipped: "
                        << outcomes[index].error->message << '\n';
                    continue;
                }

                if (index < context.textureMeans.size())
                {
                    context.textureMeans[index] = outcomes[index].cooked->mean;
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
                material.height = remapTexture(remap, material.height);

                if (material.height == assets::k_noTexture)
                {
                    material.factors.heightScale = 0.0f;
                }
            }

            reportTextures(context, textures, outcomes);

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

        const SurfaceRuleReport surfaces = applySurfaceRules(scene.value(), options.surfaces);

        std::cout << std::fixed << std::setprecision(2) << "LockstepCooker: '" << options.source.string() << "' -> '"
            << prefix << "/scene': " << scene.value().meshes.size() << " meshes, " << scene.value().instances.size()
            << " instances, " << scene.value().materials.size() << " materials, " << scene.value().textures.size()
            << " textures (read in " << secondsSince(start) << " s); " << surfaces.heightMaps
            << " materials with height maps, " << surfaces.detailLayers << " with detail layers\n";

        assets::FileSystem woundFiles;
        std::optional<SourceScene> woundScene;

        if (options.splitLimbs && !options.wound.empty())
        {
            const std::filesystem::path woundFolder = options.wound.has_parent_path() ? options.wound.parent_path() : ".";

            if (auto mounted = woundFiles.mountDirectory(woundFolder); !mounted)
            {
                return mounted;
            }

            auto read = readSourceScene(woundFiles, options.wound.filename().string(), 1.0f);

            if (!read)
            {
                return read.error();
            }

            woundScene = std::move(read).value();
        }

        CookContext context{.options = options, .sourceFiles = sourceFiles, .scene = scene.value(), .prefix = prefix,
            .level = {}, .writtenBytes = 0, .woundMaterial = k_noMaterial, .limbs = {},
            .woundScene = woundScene ? &*woundScene : nullptr, .woundFiles = &woundFiles, .woundTemplate = {},
            .woundMesh = 0, .woundTextures = {}, .textureMeans = {}};

        if (woundScene)
        {
            context.woundTemplate = woundTemplateOf(*woundScene, context.woundMesh);

            if (!context.woundTemplate)
            {
                return Error{ErrorCode::InvalidArgument, "'" + options.wound.string() + "' has no mesh on a node named '"
                    + std::string(k_woundNode) + "'"};
            }

            std::cout << "  wound stump: " << context.woundTemplate->vertices.size() << " vertices from '"
                << options.wound.string() << "'\n";
        }
        context.level.materials = scene.value().materials;

        if (options.singleSided)
        {
            uint32_t changed = 0;

            for (assets::CookedMaterial& material : context.level.materials)
            {
                if (material.factors.doubleSided && material.factors.alphaMode == render::AlphaMode::Opaque)
                {
                    material.factors.doubleSided = false;
                    ++changed;
                }
            }

            std::cout << "  single-sided: " << changed << " opaque materials no longer double-sided\n";
        }
        context.level.instances = scene.value().instances;
        context.level.nodes = scene.value().nodes;
        context.level.lights = scene.value().lights;
        context.level.bounds = scene.value().bounds;

        if (auto cooked = cookMeshes(context); !cooked)
        {
            return cooked;
        }

        if (auto cooked = cookSkinnedModels(context); !cooked)
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

        if (options.irradiance)
        {
            auto volume = bakeIrradiance(scene.value(), context.textureMeans, *options.irradiance, options.threads);

            if (!volume)
            {
                return volume.error();
            }

            if (auto written = writeFile(context, prefix + "/" + std::string(assets::k_irradianceFileName),
                    assets::writeCookedIrradiance(volume.value())); !written)
            {
                return written;
            }
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
