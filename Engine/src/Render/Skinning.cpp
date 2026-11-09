#include "Lockstep/Render/Renderer.hpp"

#include "RendererState.hpp"
#include "Skinning.hpp"
#include "VelaConversions.hpp"

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <span>

namespace lockstep::render
{
    namespace
    {
        using vela::graphics::VertexAttributeFormat;

        constexpr uint32_t k_matricesPerJoint{2};

        uint32_t offsetOf(std::size_t offset)
        {
            return static_cast<uint32_t>(offset);
        }

        math::Aabb skinnedBounds(std::span<const SkinnedPbrVertex> vertices)
        {
            math::Aabb bounds{};

            for (const SkinnedPbrVertex& vertex : vertices)
            {
                math::expand(bounds, math::Vec3{vertex.position[0], vertex.position[1], vertex.position[2]});
            }

            return bounds;
        }

        bool validJoints(const SkinnedPbrVertex& vertex, uint32_t& jointCount)
        {
            for (std::size_t influence = 0; influence < 4; ++influence)
            {
                const float joint = vertex.joints[influence];

                if (!(joint >= 0.0f) || joint >= static_cast<float>(k_maxPaletteJoints))
                {
                    return false;
                }

                if (vertex.weights[influence] != 0.0f)
                {
                    jointCount = std::max(jointCount, static_cast<uint32_t>(joint) + 1);
                }
            }

            return true;
        }
    } //namespace

    vela::graphics::VertexLayout skinnedPbrLayout()
    {
        return vela::graphics::VertexLayout
        {
            sizeof(SkinnedPbrVertex),
            {
                {0, VertexAttributeFormat::Float3, offsetOf(offsetof(SkinnedPbrVertex, position))},
                {1, VertexAttributeFormat::Float3, offsetOf(offsetof(SkinnedPbrVertex, normal))},
                {2, VertexAttributeFormat::Float2, offsetOf(offsetof(SkinnedPbrVertex, uv))},
                {3, VertexAttributeFormat::Float4, offsetOf(offsetof(SkinnedPbrVertex, tangent))},
                {4, VertexAttributeFormat::Float4, offsetOf(offsetof(SkinnedPbrVertex, joints))},
                {5, VertexAttributeFormat::Float4, offsetOf(offsetof(SkinnedPbrVertex, weights))}
            }
        };
    }

    vela::graphics::VertexLayout skinnedShadowLayout()
    {
        return vela::graphics::VertexLayout
        {
            sizeof(SkinnedPbrVertex),
            {
                {0, VertexAttributeFormat::Float3, offsetOf(offsetof(SkinnedPbrVertex, position))},
                {1, VertexAttributeFormat::Float4, offsetOf(offsetof(SkinnedPbrVertex, joints))},
                {2, VertexAttributeFormat::Float4, offsetOf(offsetof(SkinnedPbrVertex, weights))}
            }
        };
    }

    vela::graphics::VertexLayout skinnedMaskedShadowLayout()
    {
        return vela::graphics::VertexLayout
        {
            sizeof(SkinnedPbrVertex),
            {
                {0, VertexAttributeFormat::Float3, offsetOf(offsetof(SkinnedPbrVertex, position))},
                {1, VertexAttributeFormat::Float2, offsetOf(offsetof(SkinnedPbrVertex, uv))},
                {2, VertexAttributeFormat::Float4, offsetOf(offsetof(SkinnedPbrVertex, joints))},
                {3, VertexAttributeFormat::Float4, offsetOf(offsetof(SkinnedPbrVertex, weights))}
            }
        };
    }

    Status uploadJointPalettes(RendererState& state)
    {
        if (state.jointData.empty())
        {
            return {};
        }

        if (auto uploaded = state.jointBuffer.update(std::as_bytes(std::span<const math::Mat4>(state.jointData)));
            !uploaded)
        {
            return toError(uploaded.error());
        }

        return {};
    }

    Result<MeshHandle> Renderer::createMesh(std::span<const SkinnedPbrVertex> vertices, std::span<const uint32_t> indices)
    {
        if (vertices.empty() || indices.empty())
        {
            return Error{ErrorCode::InvalidArgument, "createMesh: vertices and indices must not be empty"};
        }

        if (std::any_of(indices.begin(), indices.end(), [&vertices](uint32_t index) { return index >= vertices.size(); }))
        {
            return Error{ErrorCode::InvalidArgument, "createMesh: index out of range"};
        }

        uint32_t jointCount = 0;

        for (const SkinnedPbrVertex& vertex : vertices)
        {
            if (!validJoints(vertex, jointCount))
            {
                return Error{ErrorCode::InvalidArgument, "createMesh: joint index out of range"};
            }
        }

        auto mesh = vela::graphics::Mesh::create(m_state->context, std::as_bytes(vertices),
            static_cast<uint32_t>(vertices.size()), indices);

        if (!mesh)
        {
            return toError(mesh.error());
        }

        return m_state->meshes.add(MeshRecord{.mesh = std::move(mesh).value(), .bounds = skinnedBounds(vertices),
            .kind = MeshKind::Skinned, .jointCount = std::max(jointCount, 1u)});
    }

    JointPalette Renderer::submitJointPalette(std::span<const math::Mat4> joints, std::span<const math::Mat4> previousJoints)
    {
        assert(joints.size() == previousJoints.size() && "submitJointPalette: joint counts differ");

        if (joints.empty() || joints.size() != previousJoints.size() || joints.size() > k_maxPaletteJoints)
        {
            return {};
        }

        std::vector<math::Mat4>& data = m_state->jointData;
        const JointPalette palette{.first = static_cast<uint32_t>(data.size()),
            .count = static_cast<uint32_t>(joints.size())};
        data.reserve(data.size() + joints.size() * k_matricesPerJoint);

        for (std::size_t joint = 0; joint < joints.size(); ++joint)
        {
            data.push_back(joints[joint]);
            data.push_back(previousJoints[joint]);
        }

        return palette;
    }

    void Renderer::submitSkinned(MeshHandle mesh, MaterialHandle material, const math::Transform& transform,
        JointPalette palette, const math::Aabb& localBounds, RenderLayers layers)
    {
        const MeshRecord* record = m_state->meshes.find(mesh);
        const MaterialRecord* materialRecord = m_state->materials.find(material);

        assert(record != nullptr && record->kind == MeshKind::Skinned && "submitSkinned: invalid skinned mesh handle");
        assert(materialRecord != nullptr && materialRecord->kind == MeshKind::Skinned
            && "submitSkinned: invalid skinned material handle");

        const bool fitsPalette = record != nullptr && palette.count >= record->jointCount
            && static_cast<std::size_t>(palette.first) + palette.count * k_matricesPerJoint <= m_state->jointData.size();

        if (record == nullptr || materialRecord == nullptr || record->kind != MeshKind::Skinned
            || materialRecord->kind != MeshKind::Skinned || !fitsPalette)
        {
            return;
        }

        const math::Mat4 model = math::toMatrix(transform);
        m_state->draws.push_back({
            .mesh = &record->mesh,
            .material = materialRecord,
            .model = model,
            .previousModel = model,
            .worldBounds = math::transformBounds(math::isEmpty(localBounds) ? record->bounds : localBounds, transform),
            .cameraDistance = 0.0f,
            .layers = layers,
            .isReflector = false,
            .jointBase = palette.first});
    }
} //namespace lockstep::render
