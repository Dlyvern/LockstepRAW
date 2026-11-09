#include "GltfSkins.hpp"

#include "GltfMeshes.hpp"

#include <algorithm>
#include <cstring>
#include <optional>

namespace lockstep::assets
{
    namespace
    {
        constexpr std::size_t k_influences{4};

        struct SkinAttributes
        {
            const cgltf_accessor* joints{nullptr};
            const cgltf_accessor* weights{nullptr};
        };

        math::Mat4 toMat4(const cgltf_float (&values)[16])
        {
            math::Mat4 matrix{};
            std::memcpy(matrix.data(), values, sizeof(values));

            return matrix;
        }

        math::Mat4 localMatrix(const cgltf_node& node)
        {
            cgltf_float values[16];
            cgltf_node_transform_local(&node, values);

            return toMat4(values);
        }

        math::Mat4 worldMatrix(const cgltf_node& node)
        {
            cgltf_float values[16];
            cgltf_node_transform_world(&node, values);

            return toMat4(values);
        }

        math::Transform toTransform(const math::Mat4& matrix)
        {
            math::Transform transform{};

            if (!math::decompose(matrix, transform.position, transform.rotation, transform.scale))
            {
                return math::Transform{};
            }

            return transform;
        }

        uint32_t depthOf(const cgltf_node* node)
        {
            uint32_t depth = 0;

            for (const cgltf_node* parent = node->parent; parent != nullptr; parent = parent->parent)
            {
                ++depth;
            }

            return depth;
        }

        const cgltf_node* nearestJointAncestor(const cgltf_node& node, const JointLookup& joints)
        {
            for (const cgltf_node* parent = node.parent; parent != nullptr; parent = parent->parent)
            {
                if (joints.contains(parent))
                {
                    return parent;
                }
            }

            return nullptr;
        }

        std::vector<math::Mat4> inverseBindMatrices(const cgltf_skin& skin)
        {
            std::vector<math::Mat4> matrices(skin.joints_count, math::Mat4{1.0f});
            const cgltf_accessor* accessor = skin.inverse_bind_matrices;

            if (accessor == nullptr || accessor->type != cgltf_type_mat4 || accessor->count < skin.joints_count)
            {
                return matrices;
            }

            std::vector<float> values(accessor->count * 16);

            if (cgltf_accessor_unpack_floats(accessor, values.data(), values.size()) != values.size())
            {
                return matrices;
            }

            for (std::size_t joint = 0; joint < matrices.size(); ++joint)
            {
                std::memcpy(matrices[joint].data(), values.data() + joint * 16, sizeof(float) * 16);
            }

            return matrices;
        }

        struct BuiltSkeleton
        {
            anim::Skeleton skeleton;
            JointLookup lookup;
            std::vector<uint32_t> remap;
        };

        BuiltSkeleton buildSkeleton(const cgltf_skin& skin, ImportWarnings& warnings)
        {
            std::vector<uint32_t> order(skin.joints_count);

            for (uint32_t index = 0; index < order.size(); ++index)
            {
                order[index] = index;
            }

            std::stable_sort(order.begin(), order.end(), [&skin](uint32_t left, uint32_t right)
            {
                return depthOf(skin.joints[left]) < depthOf(skin.joints[right]);
            });

            BuiltSkeleton built{};
            built.remap.resize(skin.joints_count);

            for (uint32_t index = 0; index < order.size(); ++index)
            {
                built.remap[order[index]] = index;
                built.lookup.emplace(skin.joints[order[index]], index);
            }

            const std::vector<math::Mat4> inverseBind = inverseBindMatrices(skin);
            const cgltf_node* rootParent = nullptr;
            bool foundRoot = false;
            built.skeleton.joints.resize(order.size());

            for (uint32_t index = 0; index < order.size(); ++index)
            {
                const cgltf_node& node = *skin.joints[order[index]];
                anim::Joint& joint = built.skeleton.joints[index];
                joint.name = node.name != nullptr ? node.name : "";
                joint.rest = toTransform(localMatrix(node));
                joint.inverseBind = inverseBind[order[index]];

                const cgltf_node* ancestor = nearestJointAncestor(node, built.lookup);

                if (ancestor == nullptr)
                {
                    if (foundRoot && node.parent != rootParent)
                    {
                        warnings.add("skin root joints with different parents share the first root's placement");
                    }

                    rootParent = foundRoot ? rootParent : node.parent;
                    foundRoot = true;
                    continue;
                }

                if (ancestor != node.parent)
                {
                    warnings.add("non-joint nodes between skin joints are ignored");
                }

                joint.parent = built.lookup.at(ancestor);
            }

            built.skeleton.rootMatrix = rootParent != nullptr ? worldMatrix(*rootParent) : math::Mat4{1.0f};

            return built;
        }

        SkinAttributes findSkinAttributes(const cgltf_primitive& primitive, ImportWarnings& warnings)
        {
            SkinAttributes attributes{};

            for (cgltf_size index = 0; index < primitive.attributes_count; ++index)
            {
                const cgltf_attribute& attribute = primitive.attributes[index];
                const bool isJoints = attribute.type == cgltf_attribute_type_joints;

                if (!isJoints && attribute.type != cgltf_attribute_type_weights)
                {
                    continue;
                }

                if (attribute.index != 0)
                {
                    warnings.add("joint influences beyond JOINTS_0 / WEIGHTS_0 are ignored");
                    continue;
                }

                (isJoints ? attributes.joints : attributes.weights) = attribute.data;
            }

            return attributes;
        }

        std::optional<std::vector<float>> unpackVec4(const cgltf_accessor* accessor, cgltf_size count)
        {
            if (accessor == nullptr || accessor->type != cgltf_type_vec4 || accessor->count != count)
            {
                return std::nullopt;
            }

            std::vector<float> values(count * k_influences);

            if (cgltf_accessor_unpack_floats(accessor, values.data(), values.size()) != values.size())
            {
                return std::nullopt;
            }

            return values;
        }

        Result<SkinnedGeometry> readSkinnedPrimitive(const cgltf_primitive& primitive, const std::vector<uint32_t>& remap,
            ImportWarnings& warnings)
        {
            auto geometry = readPrimitive(primitive, warnings);

            if (!geometry)
            {
                return geometry.error();
            }

            const SkinAttributes attributes = findSkinAttributes(primitive, warnings);
            const cgltf_size sourceCount = attributes.joints != nullptr ? attributes.joints->count : 0;
            const std::optional<std::vector<float>> joints = unpackVec4(attributes.joints, sourceCount);
            const std::optional<std::vector<float>> weights = unpackVec4(attributes.weights, sourceCount);

            if (!joints || !weights)
            {
                return Error{ErrorCode::AssetLoadFailed, "skinned primitive has no usable JOINTS_0 / WEIGHTS_0"};
            }

            const PrimitiveGeometry& source = geometry.value();
            SkinnedGeometry skinned{.vertices = {}, .indices = source.indices, .bounds = source.bounds};
            skinned.vertices.resize(source.vertices.size());

            for (std::size_t vertex = 0; vertex < source.vertices.size(); ++vertex)
            {
                const std::size_t original = source.sourceVertices.empty() ? vertex : source.sourceVertices[vertex];

                if (original >= sourceCount)
                {
                    return Error{ErrorCode::AssetLoadFailed, "skinned primitive joints do not match its positions"};
                }

                const render::PbrVertex& base = source.vertices[vertex];
                render::SkinnedPbrVertex& target = skinned.vertices[vertex];
                std::memcpy(target.position, base.position, sizeof(base.position));
                std::memcpy(target.normal, base.normal, sizeof(base.normal));
                std::memcpy(target.uv, base.uv, sizeof(base.uv));
                std::memcpy(target.tangent, base.tangent, sizeof(base.tangent));

                float total = 0.0f;

                for (std::size_t influence = 0; influence < k_influences; ++influence)
                {
                    const float weight = std::max((*weights)[original * k_influences + influence], 0.0f);
                    const auto joint = static_cast<std::size_t>((*joints)[original * k_influences + influence]);

                    if (weight > 0.0f && joint >= remap.size())
                    {
                        return Error{ErrorCode::AssetLoadFailed, "skinned primitive joint index out of range"};
                    }

                    target.joints[influence] = weight > 0.0f ? static_cast<float>(remap[joint]) : 0.0f;
                    target.weights[influence] = weight;
                    total += weight;
                }

                for (std::size_t influence = 0; influence < k_influences; ++influence)
                {
                    target.weights[influence] = total > 0.0f ? target.weights[influence] / total
                        : (influence == 0 ? 1.0f : 0.0f);
                }
            }

            return skinned;
        }

        math::Vec3 positionOf(const render::SkinnedPbrVertex& vertex)
        {
            return math::Vec3{vertex.position[0], vertex.position[1], vertex.position[2]};
        }

        void measureModel(GltfSkinnedModel& model)
        {
            const anim::Skeleton& skeleton = model.skeleton;
            const std::size_t jointCount = skeleton.joints.size();
            std::vector<math::Vec3> bindOrigins(jointCount);

            for (std::size_t joint = 0; joint < jointCount; ++joint)
            {
                const math::Vec4 origin = math::inverse(skeleton.joints[joint].inverseBind).getColumn(3);
                bindOrigins[joint] = math::Vec3{origin.x, origin.y, origin.z};
            }

            anim::Pose rest{};
            anim::resetToRest(skeleton, rest);
            std::vector<math::Mat4> jointMatrices(jointCount);
            std::vector<math::Mat4> skin(jointCount);
            anim::computeJointMatrices(skeleton, rest, jointMatrices);
            anim::computeSkinMatrices(skeleton, jointMatrices, skin);
            model.jointRadii.assign(jointCount, 0.0f);

            for (const SkinnedPart& part : model.parts)
            {
                for (const render::SkinnedPbrVertex& vertex : part.geometry.vertices)
                {
                    const math::Vec3 position = positionOf(vertex);
                    math::Vec3 posed{0.0f, 0.0f, 0.0f};

                    for (std::size_t influence = 0; influence < k_influences; ++influence)
                    {
                        const float weight = vertex.weights[influence];

                        if (weight <= 0.0f)
                        {
                            continue;
                        }

                        const auto joint = static_cast<std::size_t>(vertex.joints[influence]);
                        float& radius = model.jointRadii[joint];
                        radius = std::max(radius, math::length(position - bindOrigins[joint]));
                        posed += math::transformPoint(skin[joint], position) * weight;
                    }

                    math::expand(model.bindBounds, posed);
                }
            }
        }

        std::string modelName(const cgltf_skin& skin, const cgltf_node& node)
        {
            if (skin.name != nullptr && skin.name[0] != '\0')
            {
                return skin.name;
            }

            return node.name != nullptr ? node.name : "skinned model";
        }

        struct SkinBuild
        {
            const cgltf_skin* skin{nullptr};
            BuiltSkeleton built;
            GltfSkinnedModel model;
        };

        void addSkinnedNode(SkinBuild& build, const PlacedNode& placed, ImportWarnings& warnings)
        {
            const cgltf_mesh& mesh = *placed.node->mesh;

            for (cgltf_size index = 0; index < mesh.primitives_count; ++index)
            {
                const cgltf_primitive& primitive = mesh.primitives[index];
                auto geometry = readSkinnedPrimitive(primitive, build.built.remap, warnings);

                if (!geometry)
                {
                    const std::string name = mesh.name != nullptr ? mesh.name : "unnamed";
                    warnings.add("skinned mesh '" + name + "' primitive skipped: " + geometry.error().message);
                    continue;
                }

                build.model.parts.push_back(SkinnedPart{.geometry = std::move(geometry).value(),
                    .material = primitive.material});
            }
        }
    } //namespace

    std::vector<GltfSkinnedModel> readSkinnedModels(const cgltf_data& document, std::span<const PlacedNode> placed,
        float scale, ImportWarnings& warnings)
    {
        std::vector<SkinBuild> builds;

        for (const PlacedNode& node : placed)
        {
            const cgltf_skin* skin = node.node->skin;

            if (skin == nullptr || node.node->mesh == nullptr)
            {
                continue;
            }

            if (skin->joints_count == 0 || skin->joints_count > anim::k_maxJoints)
            {
                warnings.add("skins without joints or with more than " + std::to_string(anim::k_maxJoints)
                    + " joints are skipped");
                continue;
            }

            auto build = std::find_if(builds.begin(), builds.end(), [skin](const SkinBuild& item) { return item.skin == skin; });

            if (build == builds.end())
            {
                SkinBuild created{.skin = skin, .built = buildSkeleton(*skin, warnings), .model = {}};
                created.model.name = modelName(*skin, *node.node);
                created.model.node = node.instanceNode();
                created.model.transform.scale = math::Vec3{scale, scale, scale};
                builds.push_back(std::move(created));
                build = builds.end() - 1;
            }

            addSkinnedNode(*build, node, warnings);
        }

        std::vector<GltfSkinnedModel> models;

        for (SkinBuild& build : builds)
        {
            if (build.model.parts.empty())
            {
                continue;
            }

            build.model.skeleton = std::move(build.built.skeleton);
            build.model.clips = readClips(document, build.built.lookup, warnings);
            measureModel(build.model);
            models.push_back(std::move(build.model));
        }

        return models;
    }
} //namespace lockstep::assets
