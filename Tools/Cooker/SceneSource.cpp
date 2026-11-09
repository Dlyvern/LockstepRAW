#include "SceneSource.hpp"

#include "Assets/GltfMaterials.hpp"
#include "Assets/GltfNodes.hpp"
#include "Assets/ImportWarnings.hpp"

#include <map>
#include <optional>
#include <unordered_map>
#include <utility>

namespace lockstep::cooker
{
    namespace
    {
        struct SourceBuilder
        {
            SourceScene& scene;
            assets::ImportWarnings& warnings;
            std::map<std::pair<const cgltf_image*, TextureUsage>, uint32_t> textureSlots;
            std::unordered_map<const cgltf_material*, uint32_t> materialSlots;
            std::unordered_map<const cgltf_mesh*, std::vector<uint32_t>> meshParts;
        };

        const cgltf_image* sourceImage(const cgltf_texture_view& view)
        {
            if (view.texture == nullptr)
            {
                return nullptr;
            }

            if (view.texture->has_basisu && view.texture->basisu_image != nullptr)
            {
                return view.texture->basisu_image;
            }

            return view.texture->image;
        }

        uint32_t textureSlot(SourceBuilder& builder, const cgltf_image* image, TextureUsage usage)
        {
            if (image == nullptr)
            {
                return assets::k_noTexture;
            }

            const auto [slot, added] = builder.textureSlots.emplace(std::pair{image, usage},
                static_cast<uint32_t>(builder.scene.textures.size()));

            if (added)
            {
                builder.scene.textures.push_back(SourceTexture{.image = image, .usage = usage});
            }

            return slot->second;
        }

        assets::CookedMaterial cookMaterial(SourceBuilder& builder, const cgltf_material* material)
        {
            assets::CookedMaterial cooked{};
            cooked.factors = assets::toPbrDescription(material, assets::TextureTable{});

            if (material == nullptr)
            {
                return cooked;
            }

            const cgltf_pbr_metallic_roughness& pbr = material->pbr_metallic_roughness;
            const cgltf_image* metallicRoughness = material->has_pbr_metallic_roughness
                ? sourceImage(pbr.metallic_roughness_texture)
                : nullptr;
            const cgltf_image* occlusion = sourceImage(material->occlusion_texture);

            if (material->has_pbr_metallic_roughness)
            {
                cooked.baseColor = textureSlot(builder, sourceImage(pbr.base_color_texture), TextureUsage::Colour);
                cooked.metallicRoughness = textureSlot(builder, metallicRoughness, TextureUsage::Data);
            }

            cooked.normal = textureSlot(builder, sourceImage(material->normal_texture), TextureUsage::Normal);
            cooked.occlusion = textureSlot(builder, occlusion,
                occlusion == metallicRoughness ? TextureUsage::Data : TextureUsage::Occlusion);
            cooked.emissive = textureSlot(builder, sourceImage(material->emissive_texture), TextureUsage::Colour);

            return cooked;
        }

        uint32_t materialSlot(SourceBuilder& builder, const cgltf_material* material)
        {
            if (const auto found = builder.materialSlots.find(material); found != builder.materialSlots.end())
            {
                return found->second;
            }

            const auto slot = static_cast<uint32_t>(builder.scene.materials.size());
            builder.scene.materials.push_back(cookMaterial(builder, material));
            builder.materialSlots.emplace(material, slot);

            return slot;
        }

        const std::vector<uint32_t>& partsOf(SourceBuilder& builder, const cgltf_mesh& mesh)
        {
            if (const auto found = builder.meshParts.find(&mesh); found != builder.meshParts.end())
            {
                return found->second;
            }

            std::vector<uint32_t> parts;

            for (cgltf_size index = 0; index < mesh.primitives_count; ++index)
            {
                const cgltf_primitive& primitive = mesh.primitives[index];
                auto geometry = assets::readPrimitive(primitive, builder.warnings);

                if (!geometry)
                {
                    const std::string name = mesh.name != nullptr ? mesh.name : "unnamed";
                    builder.warnings.add("mesh '" + name + "' primitive skipped: " + geometry.error().message);
                    continue;
                }

                parts.push_back(static_cast<uint32_t>(builder.scene.meshes.size()));
                builder.scene.meshes.push_back(SourceMesh{
                    .geometry = std::move(geometry).value(),
                    .material = materialSlot(builder, primitive.material),
                    .alphaMode = primitive.material != nullptr ? assets::toAlphaMode(primitive.material->alpha_mode)
                        : render::AlphaMode::Opaque});
            }

            return builder.meshParts.emplace(&mesh, std::move(parts)).first->second;
        }

        void addNode(SourceBuilder& builder, const assets::PlacedNode& placed)
        {
            const cgltf_node& node = *placed.node;

            if (assets::hasSceneNode(node))
            {
                builder.scene.nodes.push_back(assets::SceneNode{.name = node.name, .transform = placed.transform,
                    .parent = placed.parentNode});
            }

            if (node.light != nullptr)
            {
                builder.scene.lights.push_back(assets::toSceneLight(*node.light, placed));
            }

            if (node.mesh == nullptr)
            {
                return;
            }

            for (const uint32_t part : partsOf(builder, *node.mesh))
            {
                const math::Aabb worldBounds = math::transformBounds(builder.scene.meshes[part].geometry.bounds,
                    placed.transform);
                builder.scene.instances.push_back(assets::SceneInstance{.mesh = part, .transform = placed.transform,
                    .worldBounds = worldBounds, .node = placed.instanceNode()});
                math::expand(builder.scene.bounds, worldBounds);
            }
        }
    } //namespace

    Result<SourceScene> readSourceScene(const assets::FileSystem& files, std::string_view path, float scale)
    {
        assets::ImportWarnings warnings;
        auto document = assets::loadGltfDocument(files, path, warnings);

        if (!document)
        {
            return document.error();
        }

        SourceScene scene{};
        scene.document = std::move(document).value();
        const std::size_t slash = path.find_last_of('/');
        scene.directory = slash == std::string_view::npos ? std::string{} : std::string(path.substr(0, slash));
        static_cast<void>(assets::TextureTable::collect(*scene.document, warnings));

        SourceBuilder builder{.scene = scene, .warnings = warnings, .textureSlots = {}, .materialSlots = {},
            .meshParts = {}};

        for (const assets::PlacedNode& placed : assets::placeNodes(*scene.document, scale, warnings))
        {
            addNode(builder, placed);
        }

        if (scene.instances.empty())
        {
            return Error{ErrorCode::AssetLoadFailed, "cook: '" + std::string(path) + "' has no drawable triangles"};
        }

        scene.warnings = warnings.take();

        return scene;
    }
} //namespace lockstep::cooker
