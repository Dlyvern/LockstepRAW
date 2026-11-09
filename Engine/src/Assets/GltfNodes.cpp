#include "GltfNodes.hpp"

#include <cstring>
#include <optional>

namespace lockstep::assets
{
    namespace
    {
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

        SceneLightType toLightType(cgltf_light_type type)
        {
            switch (type)
            {
                case cgltf_light_type_directional:
                {
                    return SceneLightType::Directional;
                }
                case cgltf_light_type_spot:
                {
                    return SceneLightType::Spot;
                }
                default:
                {
                    return SceneLightType::Point;
                }
            }
        }
    } //namespace

    bool hasSceneNode(const cgltf_node& node) noexcept
    {
        return node.name != nullptr && node.name[0] != '\0';
    }

    std::vector<PlacedNode> placeNodes(const cgltf_data& document, float scale, ImportWarnings& warnings)
    {
        struct PendingNode
        {
            const cgltf_node* node{nullptr};
            math::Mat4 parentWorld{};
            uint32_t parentNode{k_noNode};
        };

        std::vector<PlacedNode> placed;
        std::vector<PendingNode> pending;
        uint32_t sceneNodeCount = 0;
        const math::Mat4 root = math::scale(math::Mat4{1.0f}, math::Vec3{scale, scale, scale});
        const std::vector<const cgltf_node*> roots = rootNodes(document);

        for (auto node = roots.rbegin(); node != roots.rend(); ++node)
        {
            pending.push_back(PendingNode{.node = *node, .parentWorld = root, .parentNode = k_noNode});
        }

        while (!pending.empty())
        {
            const PendingNode current = pending.back();
            pending.pop_back();
            const cgltf_node* node = current.node;
            const math::Mat4 world = current.parentWorld * localMatrix(*node);
            uint32_t childParent = current.parentNode;

            if (const std::optional<math::Transform> transform = toTransform(world))
            {
                const uint32_t sceneNode = hasSceneNode(*node) ? sceneNodeCount++ : k_noNode;
                placed.push_back(PlacedNode{.node = node, .transform = *transform, .sceneNode = sceneNode,
                    .parentNode = current.parentNode});
                childParent = placed.back().instanceNode();
            }
            else if (node->mesh != nullptr)
            {
                warnings.add("nodes with a zero scale are skipped");
            }

            for (cgltf_size child = node->children_count; child > 0; --child)
            {
                pending.push_back(PendingNode{.node = node->children[child - 1], .parentWorld = world,
                    .parentNode = childParent});
            }
        }

        return placed;
    }

    SceneLight toSceneLight(const cgltf_light& light, const PlacedNode& placed)
    {
        SceneLight result{};
        result.type = toLightType(light.type);
        result.name = light.name != nullptr ? light.name : placed.node->name != nullptr ? placed.node->name : "";
        result.color = math::Vec3{light.color[0], light.color[1], light.color[2]};
        result.intensity = light.intensity;
        result.range = light.range;
        result.innerAngle = light.spot_inner_cone_angle;
        result.outerAngle = light.spot_outer_cone_angle;
        result.transform = placed.transform;

        return result;
    }
} //namespace lockstep::assets
