#include "Lockstep/Assets/Scene.hpp"

#include "Lockstep/Render/Renderer.hpp"

#include "GltfDocument.hpp"
#include "GltfMaterials.hpp"
#include "GltfMeshes.hpp"
#include "GltfNodes.hpp"
#include "GltfSkins.hpp"
#include "ImportWarnings.hpp"
#include "MeshProcessing.hpp"
#include "TextureImport.hpp"

#include <chrono>
#include <optional>
#include <span>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace lockstep::assets
{
    namespace
    {
        struct ImportContext
        {
            const cgltf_data& document;
            const SceneImportOptions& options;
            render::Renderer& renderer;
            const TextureTable& textures;
            ImportWarnings& warnings;
            Scene& scene;
            std::vector<std::optional<render::MaterialHandle>> materials;
            std::optional<render::MaterialHandle> defaultMaterial;
            std::vector<std::optional<render::MaterialHandle>> skinnedMaterials;
            std::optional<render::MaterialHandle> defaultSkinnedMaterial;
            std::unordered_map<const cgltf_mesh*, std::vector<uint32_t>> meshParts;
        };

        Result<render::MaterialHandle> materialFor(ImportContext& context, const cgltf_material* material,
            render::MeshKind kind = render::MeshKind::Static)
        {
            const bool isSkinned = kind == render::MeshKind::Skinned;
            std::vector<std::optional<render::MaterialHandle>>& table = isSkinned ? context.skinnedMaterials
                : context.materials;
            std::optional<render::MaterialHandle>& slot = material != nullptr
                ? table[static_cast<std::size_t>(material - context.document.materials)]
                : (isSkinned ? context.defaultSkinnedMaterial : context.defaultMaterial);

            if (!slot)
            {
                auto created = context.renderer.createMaterial(toPbrDescription(material, context.textures), kind);

                if (!created)
                {
                    return created.error();
                }

                slot = created.value();
                context.scene.resources.materials.push_back(created.value());
                ++context.scene.stats.materialCount;
            }

            return render::MaterialHandle{*slot};
        }

        Status uploadPrimitive(ImportContext& context, const cgltf_mesh& mesh, const cgltf_primitive& primitive,
            std::vector<uint32_t>& parts)
        {
            auto geometry = readPrimitive(primitive, context.warnings);

            if (!geometry)
            {
                const std::string name = mesh.name != nullptr ? mesh.name : "unnamed";
                context.warnings.add("mesh '" + name + "' primitive skipped: " + geometry.error().message);
                return {};
            }

            auto material = materialFor(context, primitive.material);

            if (!material)
            {
                return material.error();
            }

            const PrimitiveGeometry& data = geometry.value();
            auto uploaded = context.renderer.createMesh(std::span<const render::PbrVertex>(data.vertices),
                std::span<const uint32_t>(data.indices));

            if (!uploaded)
            {
                return uploaded.error();
            }

            context.scene.resources.meshes.push_back(uploaded.value());
            SceneMesh part{};
            part.mesh = uploaded.value();
            part.material = material.value();
            part.alphaMode = primitive.material != nullptr ? toAlphaMode(primitive.material->alpha_mode)
                : render::AlphaMode::Opaque;
            part.localBounds = data.bounds;
            part.triangleCount = static_cast<uint32_t>(data.indices.size() / 3);

            if (context.options.keepCollisionGeometry)
            {
                part.collisionPositions.reserve(data.vertices.size());

                for (const render::PbrVertex& vertex : data.vertices)
                {
                    part.collisionPositions.emplace_back(vertex.position[0], vertex.position[1], vertex.position[2]);
                }

                part.collisionIndices = nonDegenerateTriangles(part.collisionPositions, data.indices);
            }

            parts.push_back(static_cast<uint32_t>(context.scene.meshes.size()));
            context.scene.meshes.push_back(std::move(part));

            return {};
        }

        Result<const std::vector<uint32_t>*> partsOf(ImportContext& context, const cgltf_mesh& mesh)
        {
            if (const auto found = context.meshParts.find(&mesh); found != context.meshParts.end())
            {
                return &found->second;
            }

            std::vector<uint32_t> parts;

            for (cgltf_size index = 0; index < mesh.primitives_count; ++index)
            {
                if (auto uploaded = uploadPrimitive(context, mesh, mesh.primitives[index], parts); !uploaded)
                {
                    return uploaded.error();
                }
            }

            return &context.meshParts.emplace(&mesh, std::move(parts)).first->second;
        }

        Status addNode(ImportContext& context, const PlacedNode& placed)
        {
            const cgltf_node& node = *placed.node;
            const math::Transform& transform = placed.transform;

            if (hasSceneNode(node))
            {
                context.scene.nodes.push_back(SceneNode{.name = node.name, .transform = transform,
                    .parent = placed.parentNode});
            }

            if (node.mesh == nullptr || node.skin != nullptr)
            {
                return {};
            }

            auto parts = partsOf(context, *node.mesh);

            if (!parts)
            {
                return parts.error();
            }

            for (const uint32_t part : *parts.value())
            {
                const SceneMesh& mesh = context.scene.meshes[part];
                const math::Aabb worldBounds = math::transformBounds(mesh.localBounds, transform);
                context.scene.instances.push_back(SceneInstance{.mesh = part, .transform = transform,
                    .worldBounds = worldBounds, .node = placed.instanceNode()});
                math::expand(context.scene.bounds, worldBounds);
                context.scene.stats.triangleCount += mesh.triangleCount;
            }

            return {};
        }

        Status addSkinnedModel(ImportContext& context, const GltfSkinnedModel& source)
        {
            anim::SkinnedModel model{};
            model.name = source.name;
            model.skeleton = source.skeleton;
            model.clips = source.clips;
            model.jointRadii = source.jointRadii;
            model.bindBounds = source.bindBounds;

            for (const SkinnedPart& part : source.parts)
            {
                auto material = materialFor(context, part.material, render::MeshKind::Skinned);

                if (!material)
                {
                    return material.error();
                }

                auto mesh = context.renderer.createMesh(std::span<const render::SkinnedPbrVertex>(part.geometry.vertices),
                    std::span<const uint32_t>(part.geometry.indices));

                if (!mesh)
                {
                    return mesh.error();
                }

                context.scene.resources.meshes.push_back(mesh.value());
                model.parts.push_back(anim::SkinnedModelPart{.mesh = mesh.value(), .material = material.value(),
                    .materialName = part.material != nullptr && part.material->name != nullptr ? part.material->name : ""});
                context.scene.stats.triangleCount += part.geometry.indices.size() / 3;
            }

            context.scene.skinnedInstances.push_back(SceneSkinnedInstance{
                .model = static_cast<uint32_t>(context.scene.skinnedModels.size()), .transform = source.transform,
                .node = source.node});
            math::expand(context.scene.bounds, math::transformBounds(model.bindBounds, source.transform));
            context.scene.skinnedModels.push_back(std::move(model));

            return {};
        }

        Status addNodeTree(ImportContext& context)
        {
            const std::vector<PlacedNode> placedNodes = placeNodes(context.document, context.options.scale,
                context.warnings);

            for (const PlacedNode& placed : placedNodes)
            {
                if (placed.node->light != nullptr)
                {
                    context.scene.lights.push_back(toSceneLight(*placed.node->light, placed));
                }

                if (auto added = addNode(context, placed); !added)
                {
                    return added;
                }
            }

            for (const GltfSkinnedModel& model : readSkinnedModels(context.document, placedNodes,
                     context.options.scale, context.warnings))
            {
                if (auto added = addSkinnedModel(context, model); !added)
                {
                    return added;
                }
            }

            return {};
        }
    } //namespace

    Result<Scene> importGltf(render::Renderer& renderer, const FileSystem& files, std::string_view path,
        const SceneImportOptions& options)
    {
        const auto start = std::chrono::steady_clock::now();

        if (!(options.scale > 0.0f))
        {
            return Error{ErrorCode::InvalidArgument, "importGltf: scale must be > 0"};
        }

        ImportWarnings warnings;
        auto document = loadGltfDocument(files, path, warnings);

        if (!document)
        {
            return document.error();
        }

        const cgltf_data& data = *document.value();
        Scene scene{};
        TextureTable textures = TextureTable::collect(data, warnings);

        const std::size_t slash = path.find_last_of('/');
        const std::string_view directory = slash == std::string_view::npos ? std::string_view{} : path.substr(0, slash);

        auto imported = importTextures(renderer, textures, files, directory, options, scene.stats, warnings);

        for (const render::TextureHandle texture : textures.handles())
        {
            if (texture.isValid())
            {
                scene.resources.textures.push_back(texture);
            }
        }

        if (!imported)
        {
            releaseScene(renderer, scene);
            return imported.error();
        }

        ImportContext context{
            .document = data,
            .options = options,
            .renderer = renderer,
            .textures = textures,
            .warnings = warnings,
            .scene = scene,
            .materials = std::vector<std::optional<render::MaterialHandle>>(data.materials_count),
            .defaultMaterial = std::nullopt,
            .skinnedMaterials = std::vector<std::optional<render::MaterialHandle>>(data.materials_count),
            .defaultSkinnedMaterial = std::nullopt,
            .meshParts = {}};

        if (auto added = addNodeTree(context); !added)
        {
            releaseScene(renderer, scene);
            return added.error();
        }

        if (scene.instances.empty() && scene.skinnedInstances.empty())
        {
            releaseScene(renderer, scene);
            return Error{ErrorCode::AssetLoadFailed, "importGltf: '" + std::string(path) + "' has no drawable triangles"};
        }

        scene.warnings = warnings.take();
        scene.stats.meshCount = static_cast<uint32_t>(scene.meshes.size());
        scene.stats.instanceCount = static_cast<uint32_t>(scene.instances.size());
        scene.stats.loadSeconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();

        return scene;
    }

    Result<Scene> importGltf(render::Renderer& renderer, const std::filesystem::path& path,
        const SceneImportOptions& options)
    {
        FileSystem files;

        if (auto mounted = files.mountDirectory(path.has_parent_path() ? path.parent_path() : "."); !mounted)
        {
            return mounted.error();
        }

        return importGltf(renderer, files, path.filename().string(), options);
    }
} //namespace lockstep::assets
