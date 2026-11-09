#include "MeshProcessing.hpp"

#include <cmath>

namespace lockstep::assets
{
    namespace
    {
        constexpr float k_minimumLengthSquared{1.0e-20f};
        constexpr float k_minimumUvArea{1.0e-12f};

        math::Vec3 position(const render::PbrVertex& vertex)
        {
            return math::Vec3{vertex.position[0], vertex.position[1], vertex.position[2]};
        }

        math::Vec3 normal(const render::PbrVertex& vertex)
        {
            return math::Vec3{vertex.normal[0], vertex.normal[1], vertex.normal[2]};
        }

        math::Vec3 anyPerpendicular(const math::Vec3& direction)
        {
            const math::Vec3 axis = std::abs(direction.x) < 0.9f ? math::Vec3{1.0f, 0.0f, 0.0f}
                : math::Vec3{0.0f, 1.0f, 0.0f};

            return math::normalize(math::cross(axis, direction));
        }
    } //namespace

    void generateFlatNormals(std::vector<render::PbrVertex>& vertices, std::vector<uint32_t>& indices)
    {
        std::vector<render::PbrVertex> flat;
        flat.reserve(indices.size());

        for (std::size_t first = 0; first + 2 < indices.size(); first += 3)
        {
            const render::PbrVertex corners[3]{vertices[indices[first]], vertices[indices[first + 1]],
                vertices[indices[first + 2]]};
            const math::Vec3 faceNormal = math::cross(position(corners[1]) - position(corners[0]),
                position(corners[2]) - position(corners[0]));
            const math::Vec3 unit = math::lengthSquared(faceNormal) > k_minimumLengthSquared
                ? math::normalize(faceNormal)
                : math::Vec3{0.0f, 1.0f, 0.0f};

            for (render::PbrVertex corner : corners)
            {
                corner.normal[0] = unit.x;
                corner.normal[1] = unit.y;
                corner.normal[2] = unit.z;
                flat.push_back(corner);
            }
        }

        vertices = std::move(flat);
        indices.resize(vertices.size());

        for (std::size_t index = 0; index < indices.size(); ++index)
        {
            indices[index] = static_cast<uint32_t>(index);
        }
    }

    void generateTangents(std::span<render::PbrVertex> vertices, std::span<const uint32_t> indices)
    {
        std::vector<math::Vec3> tangents(vertices.size(), math::Vec3{0.0f, 0.0f, 0.0f});
        std::vector<math::Vec3> bitangents(vertices.size(), math::Vec3{0.0f, 0.0f, 0.0f});

        for (std::size_t first = 0; first + 2 < indices.size(); first += 3)
        {
            const uint32_t corner0 = indices[first];
            const uint32_t corner1 = indices[first + 1];
            const uint32_t corner2 = indices[first + 2];
            const render::PbrVertex& vertex0 = vertices[corner0];
            const render::PbrVertex& vertex1 = vertices[corner1];
            const render::PbrVertex& vertex2 = vertices[corner2];

            const math::Vec3 edge1 = position(vertex1) - position(vertex0);
            const math::Vec3 edge2 = position(vertex2) - position(vertex0);
            const float du1 = vertex1.uv[0] - vertex0.uv[0];
            const float dv1 = vertex1.uv[1] - vertex0.uv[1];
            const float du2 = vertex2.uv[0] - vertex0.uv[0];
            const float dv2 = vertex2.uv[1] - vertex0.uv[1];
            const float determinant = du1 * dv2 - du2 * dv1;

            if (std::abs(determinant) < k_minimumUvArea)
            {
                continue;
            }

            const float inverse = 1.0f / determinant;
            const math::Vec3 alongU = (edge1 * dv2 - edge2 * dv1) * inverse;
            const math::Vec3 alongV = (edge2 * du1 - edge1 * du2) * inverse;

            for (const uint32_t corner : {corner0, corner1, corner2})
            {
                tangents[corner] += alongU;
                bitangents[corner] += alongV;
            }
        }

        for (std::size_t index = 0; index < vertices.size(); ++index)
        {
            render::PbrVertex& vertex = vertices[index];
            const math::Vec3 unitNormal = normal(vertex);
            const math::Vec3 projected = tangents[index] - unitNormal * math::dot(unitNormal, tangents[index]);
            const math::Vec3 tangent = math::lengthSquared(projected) > k_minimumLengthSquared
                ? math::normalize(projected)
                : anyPerpendicular(unitNormal);
            const float handedness = math::dot(math::cross(unitNormal, tangent), bitangents[index]) > 0.0f ? -1.0f : 1.0f;

            vertex.tangent[0] = tangent.x;
            vertex.tangent[1] = tangent.y;
            vertex.tangent[2] = tangent.z;
            vertex.tangent[3] = handedness;
        }
    }

    std::vector<uint32_t> nonDegenerateTriangles(std::span<const math::Vec3> positions,
        std::span<const uint32_t> indices)
    {
        std::vector<uint32_t> kept;
        kept.reserve(indices.size());

        for (std::size_t first = 0; first + 2 < indices.size(); first += 3)
        {
            const uint32_t corner0 = indices[first];
            const uint32_t corner1 = indices[first + 1];
            const uint32_t corner2 = indices[first + 2];

            if (corner0 == corner1 || corner1 == corner2 || corner0 == corner2)
            {
                continue;
            }

            const math::Vec3 area = math::cross(positions[corner1] - positions[corner0],
                positions[corner2] - positions[corner0]);

            if (math::lengthSquared(area) <= k_minimumLengthSquared)
            {
                continue;
            }

            kept.insert(kept.end(), {corner0, corner1, corner2});
        }

        return kept;
    }
} //namespace lockstep::assets
