#include "Lockstep/Assets/Scene.hpp"

#include "Lockstep/Render/Renderer.hpp"

#include "GltfDocument.hpp"
#include "GltfMaterials.hpp"
#include "GltfMeshes.hpp"
#include "ImportWarnings.hpp"
#include "MeshProcessing.hpp"
#include "TextureImport.hpp"

#include <chrono>
#include <cstring>
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
            std::unordered_map<const cgltf_mesh*, std::vector<uint32_t>> meshParts;
        };

        Result<render::MaterialHandle> materialFor(ImportContext& context, const cgltf_material* material)
        {
            std::optional<render::MaterialHandle>& slot = material != nullptr
                ? context.materials[static_cast<std::size_t>(material - context.document.materials)]
                : context.defaultMaterial;

            if (!slot)
            {
                auto created = context.renderer.createMaterial(toPbrDescription(material, context.textures));

                if (!created)
                {
                    return created.error();
                }

                slot = created.value();
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

        math::Mat4 localMatrix(const cgltf_node& node)
        {
            cgltf_float values[16];
            cgltf_node_transform_local(&node, values);
            math::Mat4 matrix{};
            std::memcpy(matrix.data(), values, sizeof(values));

            return matrix;
        }

        std::optional<math::Transform> toTransform(const math::Mat4& matrix)
        {
            math::Transform transform{};

            if (!math::decompose(matrix, transform.position, transform.rotation, transform.scale))
            {
                return std::nullopt;
            }

            return transform;
        }

        Status addNode(ImportContext& context, const cgltf_node& node, const math::Transform& transform)
        {
            if (node.name != nullptr && node.name[0] != '\0')
            {
                context.scene.nodes.push_back(SceneNode{.name = node.name, .transform = transform});
            }

            if (node.mesh == nullptr)
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
                    .worldBounds = worldBounds});
                math::expand(context.scene.bounds, worldBounds);
                context.scene.stats.triangleCount += mesh.triangleCount;
            }

            return {};
        }

        std::vector<const cgltf_node*> rootNodes(const cgltf_data& document)
        {
            std::vector<const cgltf_node*> roots;
            const cgltf_scene* scene = document.scene != nullptr ? document.scene
                : document.scenes_count > 0 ? &document.scenes[0] : nullptr;

            if (scene != nullptr)
            {
                roots.assign(scene->nodes, scene->nodes + scene->nodes_count);
                return roots;
            }

            for (cgltf_size index = 0; index < document.nodes_count; ++index)
            {
                if (document.nodes[index].parent == nullptr)
                {
                    roots.push_back(&document.nodes[index]);
                }
            }

            return roots;
        }

        Status addNodeTree(ImportContext& context)
        {
            const float scale = context.options.scale;
            std::vector<std::pair<const cgltf_node*, math::Mat4>> pending;
            const math::Mat4 root = math::scale(math::Mat4{1.0f}, math::Vec3{scale, scale, scale});

            for (const cgltf_node* node : rootNodes(context.document))
            {
                pending.emplace_back(node, root);
            }

            while (!pending.empty())
            {
                const auto [node, parent] = pending.back();
                pending.pop_back();
                const math::Mat4 world = parent * localMatrix(*node);

                if (node->skin != nullptr)
                {
                    context.warnings.add("skins are ignored; skinned meshes are drawn in bind pose");
                }

                if (const std::optional<math::Transform> transform = toTransform(world))
                {
                    if (auto added = addNode(context, *node, *transform); !added)
                    {
                        return added;
                    }
                }
                else if (node->mesh != nullptr)
                {
                    context.warnings.add("nodes with a zero scale are skipped");
                }

                for (cgltf_size child = node->children_count; child > 0; --child)
                {
                    pending.emplace_back(node->children[child - 1], world);
                }
            }

            return {};
        }
    } //namespace

    Result<Scene> importGltf(render::Renderer& renderer, const std::filesystem::path& path,
        const SceneImportOptions& options)
    {
        const auto start = std::chrono::steady_clock::now();

        if (!(options.scale > 0.0f))
        {
            return Error{ErrorCode::InvalidArgument, "importGltf: scale must be > 0"};
        }

        ImportWarnings warnings;
        auto document = loadGltfDocument(path, warnings);

        if (!document)
        {
            return document.error();
        }

        const cgltf_data& data = *document.value();
        Scene scene{};
        TextureTable textures = TextureTable::collect(data, warnings);

        if (auto imported = importTextures(renderer, textures, path.parent_path(), options, scene.stats, warnings);
            !imported)
        {
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
            .meshParts = {}};

        if (auto added = addNodeTree(context); !added)
        {
            return added.error();
        }

        if (scene.instances.empty())
        {
            return Error{ErrorCode::AssetLoadFailed, "importGltf: '" + path.string() + "' has no drawable triangles"};
        }

        scene.warnings = warnings.take();
        scene.stats.meshCount = static_cast<uint32_t>(scene.meshes.size());
        scene.stats.instanceCount = static_cast<uint32_t>(scene.instances.size());
        scene.stats.loadSeconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();

        return scene;
    }
} //namespace lockstep::assets
