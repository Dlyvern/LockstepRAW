#include "Lockstep/Assets/FileSystem.hpp"
#include "Lockstep/Assets/Scene.hpp"
#include "Lockstep/Core/Log.hpp"
#include "Lockstep/Render/Renderer.hpp"

#include "CookedFormat.hpp"
#include "CookedIrradiance.hpp"
#include "DecodedTexture.hpp"
#include "OrderedWork.hpp"

#include <chrono>
#include <optional>
#include <string>
#include <vector>

namespace lockstep::assets
{
    namespace
    {
        constexpr std::size_t k_filesPerWorker{2};

        struct LoadContext
        {
            render::Renderer& renderer;
            const FileSystem& files;
            uint32_t workerThreads{0};
            Scene& scene;
            std::optional<Error> error;
        };

        template<typename T, typename Parse, typename Use>
        Status loadAll(LoadContext& context, const std::vector<std::string>& paths, Parse parse, Use use)
        {
            const uint32_t workers = workerCountFor(context.workerThreads, paths.size());

            const auto produce = [&context, &paths, &parse](std::size_t index) -> Result<T>
            {
                auto bytes = context.files.read(paths[index]);

                if (!bytes)
                {
                    return bytes.error();
                }

                return parse(std::move(bytes).value(), paths[index]);
            };

            const auto consume = [&context, &use](std::size_t index, Result<T>&& loaded)
            {
                if (context.error)
                {
                    return;
                }

                if (!loaded)
                {
                    context.error = loaded.error();
                    return;
                }

                if (auto used = use(index, std::move(loaded).value()); !used)
                {
                    context.error = used.error();
                }
            };

            runOrdered<Result<T>>(paths.size(), workers, workers * k_filesPerWorker, produce, consume);

            if (context.error)
            {
                return *context.error;
            }

            return {};
        }

        struct LoadedTexture
        {
            std::vector<std::byte> bytes;
            CookedTextureView view{};
        };

        Result<std::vector<render::TextureHandle>> loadTextures(LoadContext& context, const CookedLevel& level)
        {
            std::vector<render::TextureHandle> handles(level.textures.size());
            const bool blockCompression = context.renderer.supportsBlockCompression();
            SceneStats& stats = context.scene.stats;

            const auto parse = [](std::vector<std::byte> bytes, const std::string& path) -> Result<LoadedTexture>
            {
                auto view = readCookedTexture(bytes, path);

                if (!view)
                {
                    return view.error();
                }

                const CookedTextureView fields = view.value();
                return LoadedTexture{.bytes = std::move(bytes), .view = fields};
            };

            const auto use = [&](std::size_t index, LoadedTexture texture) -> Status
            {
                const CookedTextureHeader& header = texture.view.header;
                const auto format = static_cast<render::TextureFormat>(header.format);

                if (isBlockCompressed(format) && !blockCompression)
                {
                    return Error{ErrorCode::InvalidArgument, "texture '" + level.textures[index]
                        + "' is block compressed, but the GPU has no Bc support"};
                }

                auto handle = context.renderer.createTexture(render::TextureDescription{
                    .pixels = texture.view.pixels,
                    .width = header.width,
                    .height = header.height,
                    .format = format,
                    .filter = render::TextureFilter::Linear,
                    .levels = header.levels,
                    .generateMips = false});

                if (!handle)
                {
                    return handle.error();
                }

                handles[index] = handle.value();
                context.scene.resources.textures.push_back(handle.value());
                ++stats.textureCount;
                stats.textureBytes += texture.view.pixels.size();
                stats.compressedTextures += isBlockCompressed(format) ? 1u : 0u;
                stats.contentBytes += texture.bytes.size();

                return {};
            };

            if (auto loaded = loadAll<LoadedTexture>(context, level.textures, parse, use); !loaded)
            {
                return loaded.error();
            }

            return handles;
        }

        render::TextureHandle textureAt(const std::vector<render::TextureHandle>& textures, uint32_t index)
        {
            return index < textures.size() ? textures[index] : render::TextureHandle{};
        }

        render::PbrMaterialDescription describe(const CookedMaterial& material,
            const std::vector<render::TextureHandle>& textures)
        {
            render::PbrMaterialDescription description = material.factors;
            description.baseColor = textureAt(textures, material.baseColor);
            description.normal = textureAt(textures, material.normal);
            description.metallicRoughness = textureAt(textures, material.metallicRoughness);
            description.occlusion = textureAt(textures, material.occlusion);
            description.emissive = textureAt(textures, material.emissive);
            description.height = textureAt(textures, material.height);

            return description;
        }

        Result<std::vector<render::MaterialHandle>> createMaterials(LoadContext& context, const CookedLevel& level,
            const std::vector<render::TextureHandle>& textures)
        {
            std::vector<render::MaterialHandle> materials(level.materials.size());

            for (const CookedMesh& mesh : level.meshes)
            {
                if (materials[mesh.material].isValid())
                {
                    continue;
                }

                auto created = context.renderer.createMaterial(describe(level.materials[mesh.material], textures));

                if (!created)
                {
                    return created.error();
                }

                materials[mesh.material] = created.value();
                context.scene.resources.materials.push_back(created.value());
                ++context.scene.stats.materialCount;
            }

            return materials;
        }

        Status loadMeshes(LoadContext& context, const CookedLevel& level,
            const std::vector<render::MaterialHandle>& materials)
        {
            std::vector<std::string> paths;
            paths.reserve(level.meshes.size());

            for (const CookedMesh& mesh : level.meshes)
            {
                paths.push_back(mesh.path);
            }

            context.scene.meshes.resize(level.meshes.size());

            const auto parse = [&context](std::vector<std::byte> bytes, const std::string& path)
            {
                return readCookedMesh(bytes, path);
            };

            const auto use = [&](std::size_t index, CookedMeshData data) -> Status
            {
                auto uploaded = context.renderer.createMesh(std::span<const render::PbrVertex>(data.vertices),
                    std::span<const uint32_t>(data.indices));

                if (!uploaded)
                {
                    return uploaded.error();
                }

                context.scene.resources.meshes.push_back(uploaded.value());
                const CookedMesh& cooked = level.meshes[index];
                SceneMesh& mesh = context.scene.meshes[index];
                mesh.mesh = uploaded.value();
                mesh.material = materials[cooked.material];
                mesh.alphaMode = cooked.alphaMode;
                const math::Vec3& emission = level.materials[cooked.material].factors.emissiveFactor;
                mesh.isEmissive = emission.x > 0.0f || emission.y > 0.0f || emission.z > 0.0f;
                mesh.localBounds = cooked.localBounds;
                mesh.triangleCount = cooked.triangleCount;
                mesh.collisionShape = cooked.collisionShape;
                context.scene.stats.contentBytes += data.vertices.size() * sizeof(render::PbrVertex)
                    + data.indices.size() * sizeof(uint32_t);

                return {};
            };

            return loadAll<CookedMeshData>(context, paths, parse, use);
        }

        Status loadCollisionShapes(LoadContext& context, const CookedLevel& level)
        {
            context.scene.collisionShapes.resize(level.collisionShapes.size());

            const auto parse = [](std::vector<std::byte> bytes, const std::string& path)
            {
                return readCookedCollision(std::move(bytes), path);
            };

            const auto use = [&context](std::size_t index, std::vector<std::byte> shape) -> Status
            {
                context.scene.stats.contentBytes += shape.size();
                context.scene.collisionShapes[index] = std::move(shape);
                return {};
            };

            return loadAll<std::vector<std::byte>>(context, level.collisionShapes, parse, use);
        }

        Result<render::MaterialHandle> skinnedMaterial(LoadContext& context, const CookedLevel& level,
            const std::vector<render::TextureHandle>& textures, std::vector<render::MaterialHandle>& created,
            uint32_t index)
        {
            if (created[index].isValid())
            {
                return render::MaterialHandle{created[index]};
            }

            auto material = context.renderer.createMaterial(describe(level.materials[index], textures),
                render::MeshKind::Skinned);

            if (!material)
            {
                return material.error();
            }

            created[index] = material.value();
            context.scene.resources.materials.push_back(material.value());
            ++context.scene.stats.materialCount;

            return render::MaterialHandle{created[index]};
        }

        Result<anim::SkinnedModel> loadSkinnedModel(LoadContext& context, const CookedLevel& level,
            const std::vector<render::TextureHandle>& textures, std::vector<render::MaterialHandle>& materials,
            const std::string& path)
        {
            auto bytes = context.files.read(path);

            if (!bytes)
            {
                return bytes.error();
            }

            context.scene.stats.contentBytes += bytes.value().size();
            auto cooked = readCookedModel(bytes.value(), path);

            if (!cooked)
            {
                return cooked.error();
            }

            CookedModel& source = cooked.value();
            anim::SkinnedModel model{.name = std::move(source.name), .skeleton = std::move(source.skeleton), .parts = {},
                .clips = std::move(source.clips), .jointRadii = std::move(source.jointRadii),
                .bindBounds = source.bindBounds, .limbs = std::move(source.limbs)};

            for (const CookedSkinnedPart& part : source.parts)
            {
                if (part.material >= level.materials.size())
                {
                    return Error{ErrorCode::AssetLoadFailed, "model '" + path + "': a material index is out of range"};
                }

                auto meshBytes = context.files.read(part.path);

                if (!meshBytes)
                {
                    return meshBytes.error();
                }

                context.scene.stats.contentBytes += meshBytes.value().size();
                auto data = readCookedSkinnedMesh(meshBytes.value(), part.path);

                if (!data)
                {
                    return data.error();
                }

                auto material = skinnedMaterial(context, level, textures, materials, part.material);

                if (!material)
                {
                    return material.error();
                }

                auto mesh = context.renderer.createMesh(std::span<const render::SkinnedPbrVertex>(data.value().vertices),
                    std::span<const uint32_t>(data.value().indices));

                if (!mesh)
                {
                    return mesh.error();
                }

                context.scene.resources.meshes.push_back(mesh.value());
                model.parts.push_back(anim::SkinnedModelPart{.mesh = mesh.value(), .material = material.value(),
                    .materialName = part.materialName, .limb = part.limb, .role = part.role});
                context.scene.stats.triangleCount += data.value().indices.size() / 3;
            }

            return model;
        }

        Status loadSkinnedModels(LoadContext& context, const CookedLevel& level,
            const std::vector<render::TextureHandle>& textures)
        {
            std::vector<render::MaterialHandle> materials(level.materials.size());

            for (const std::string& path : level.models)
            {
                auto model = loadSkinnedModel(context, level, textures, materials, path);

                if (!model)
                {
                    return model.error();
                }

                context.scene.skinnedModels.push_back(std::move(model).value());
            }

            context.scene.skinnedInstances = level.skinnedInstances;

            return {};
        }

        void placeInstances(Scene& scene, CookedLevel& level)
        {
            scene.instances = std::move(level.instances);
            scene.nodes = std::move(level.nodes);
            scene.lights = std::move(level.lights);
            scene.bounds = level.bounds;

            for (const SceneInstance& instance : scene.instances)
            {
                scene.stats.triangleCount += scene.meshes[instance.mesh].triangleCount;
            }

            scene.stats.meshCount = static_cast<uint32_t>(scene.meshes.size());
            scene.stats.instanceCount = static_cast<uint32_t>(scene.instances.size());
        }

        std::optional<render::IrradianceVolume> loadIrradiance(const FileSystem& files, std::string_view levelPath)
        {
            const std::size_t slash = levelPath.rfind('/');
            const std::string folder(slash == std::string_view::npos ? std::string_view{} : levelPath.substr(0, slash + 1));
            const std::string path = folder + std::string(k_irradianceFileName);

            if (!files.exists(path))
            {
                return std::nullopt;
            }

            auto bytes = files.read(path);
            auto volume = bytes ? readCookedIrradiance(bytes.value(), path)
                : Result<render::IrradianceVolume>(bytes.error());

            if (!volume)
            {
                log(LogLevel::Warning, "Assets", "irradiance skipped: " + volume.error().message);
                return std::nullopt;
            }

            log(LogLevel::Info, "Assets", "irradiance volume '" + path + "': " + std::to_string(volume.value().countX)
                + " x " + std::to_string(volume.value().countY) + " x " + std::to_string(volume.value().countZ) + " probes");

            return std::move(volume).value();
        }
    } //namespace

    Result<Scene> loadCookedScene(render::Renderer& renderer, const FileSystem& files, std::string_view path,
        const CookedSceneOptions& options)
    {
        const auto start = std::chrono::steady_clock::now();
        auto bytes = files.read(path);

        if (!bytes)
        {
            return bytes.error();
        }

        auto level = readCookedLevel(bytes.value(), path);

        if (!level)
        {
            return level.error();
        }

        Scene scene{};
        scene.stats.contentBytes = bytes.value().size();
        LoadContext context{.renderer = renderer, .files = files, .workerThreads = options.workerThreads,
            .scene = scene, .error = std::nullopt};
        const auto release = [&renderer, &scene](Error error)
        {
            releaseScene(renderer, scene);
            return error;
        };
        auto textures = loadTextures(context, level.value());

        if (!textures)
        {
            return release(textures.error());
        }

        auto materials = createMaterials(context, level.value(), textures.value());

        if (!materials)
        {
            return release(materials.error());
        }

        if (auto loaded = loadMeshes(context, level.value(), materials.value()); !loaded)
        {
            return release(loaded.error());
        }

        if (auto loaded = loadCollisionShapes(context, level.value()); !loaded)
        {
            return release(loaded.error());
        }

        if (auto loaded = loadSkinnedModels(context, level.value(), textures.value()); !loaded)
        {
            return release(loaded.error());
        }

        placeInstances(scene, level.value());
        scene.irradiance = loadIrradiance(files, path);
        scene.stats.loadSeconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();

        return scene;
    }
} //namespace lockstep::assets
