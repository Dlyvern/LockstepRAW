#include "GltfMeshes.hpp"

#include "MeshProcessing.hpp"

#include <span>
#include <string>

namespace lockstep::assets
{
    namespace
    {
        struct Attributes
        {
            const cgltf_accessor* position{nullptr};
            const cgltf_accessor* normal{nullptr};
            const cgltf_accessor* uv{nullptr};
            const cgltf_accessor* tangent{nullptr};
        };

        Error primitiveError(std::string message)
        {
            return Error{ErrorCode::AssetLoadFailed, std::move(message)};
        }

        Attributes findAttributes(const cgltf_primitive& primitive, ImportWarnings& warnings)
        {
            Attributes attributes{};

            for (cgltf_size index = 0; index < primitive.attributes_count; ++index)
            {
                const cgltf_attribute& attribute = primitive.attributes[index];

                switch (attribute.type)
                {
                    case cgltf_attribute_type_position:
                    {
                        attributes.position = attribute.data;
                        break;
                    }
                    case cgltf_attribute_type_normal:
                    {
                        attributes.normal = attribute.data;
                        break;
                    }
                    case cgltf_attribute_type_tangent:
                    {
                        attributes.tangent = attribute.data;
                        break;
                    }
                    case cgltf_attribute_type_texcoord:
                    {
                        if (attribute.index == 0)
                        {
                            attributes.uv = attribute.data;
                        }
                        else
                        {
                            warnings.add("texture coordinate sets other than TEXCOORD_0 are ignored");
                        }

                        break;
                    }
                    default:
                    {
                        break;
                    }
                }
            }

            if (primitive.targets_count > 0)
            {
                warnings.add("morph targets are ignored");
            }

            return attributes;
        }

        std::vector<float> unpack(const cgltf_accessor& accessor, cgltf_size components)
        {
            std::vector<float> values(accessor.count * components);
            const cgltf_size floats = cgltf_accessor_unpack_floats(&accessor, values.data(), values.size());
            values.resize(floats);

            return values;
        }

        bool fillAttribute(const cgltf_accessor* accessor, cgltf_type type, cgltf_size components,
            std::vector<render::PbrVertex>& vertices, float* (*field)(render::PbrVertex&))
        {
            if (accessor == nullptr || accessor->type != type || accessor->count != vertices.size())
            {
                return false;
            }

            const std::vector<float> values = unpack(*accessor, components);

            if (values.size() != vertices.size() * components)
            {
                return false;
            }

            for (std::size_t vertex = 0; vertex < vertices.size(); ++vertex)
            {
                float* destination = field(vertices[vertex]);

                for (cgltf_size component = 0; component < components; ++component)
                {
                    destination[component] = values[vertex * components + component];
                }
            }

            return true;
        }

        std::vector<uint32_t> readIndices(const cgltf_primitive& primitive, cgltf_size vertexCount)
        {
            std::vector<uint32_t> indices;

            if (primitive.indices == nullptr)
            {
                indices.resize(vertexCount);

                for (cgltf_size index = 0; index < vertexCount; ++index)
                {
                    indices[index] = static_cast<uint32_t>(index);
                }

                return indices;
            }

            const cgltf_accessor& accessor = *primitive.indices;
            indices.resize(accessor.count);

            if (cgltf_accessor_unpack_indices(&accessor, indices.data(), sizeof(uint32_t), indices.size()) != indices.size())
            {
                for (cgltf_size index = 0; index < accessor.count; ++index)
                {
                    indices[index] = static_cast<uint32_t>(cgltf_accessor_read_index(&accessor, index));
                }
            }

            return indices;
        }

        std::vector<uint32_t> toTriangleList(cgltf_primitive_type type, const std::vector<uint32_t>& indices)
        {
            if (type == cgltf_primitive_type_triangles)
            {
                std::vector<uint32_t> list(indices.begin(), indices.end() - static_cast<std::ptrdiff_t>(indices.size() % 3));
                return list;
            }

            std::vector<uint32_t> list;

            for (std::size_t index = 2; index < indices.size(); ++index)
            {
                if (type == cgltf_primitive_type_triangle_fan)
                {
                    list.insert(list.end(), {indices[0], indices[index - 1], indices[index]});
                }
                else if (index % 2 == 0)
                {
                    list.insert(list.end(), {indices[index - 2], indices[index - 1], indices[index]});
                }
                else
                {
                    list.insert(list.end(), {indices[index - 1], indices[index - 2], indices[index]});
                }
            }

            return list;
        }

        bool isTriangleType(cgltf_primitive_type type)
        {
            return type == cgltf_primitive_type_triangles || type == cgltf_primitive_type_triangle_strip
                || type == cgltf_primitive_type_triangle_fan;
        }
    } //namespace

    Result<PrimitiveGeometry> readPrimitive(const cgltf_primitive& primitive, ImportWarnings& warnings)
    {
        if (primitive.has_draco_mesh_compression)
        {
            return primitiveError("Draco-compressed primitives are not supported");
        }

        if (!isTriangleType(primitive.type))
        {
            return primitiveError("point and line primitives are not supported");
        }

        const Attributes attributes = findAttributes(primitive, warnings);

        if (attributes.position == nullptr || attributes.position->type != cgltf_type_vec3
            || attributes.position->count == 0)
        {
            return primitiveError("primitive has no VEC3 POSITION");
        }

        PrimitiveGeometry geometry{};
        geometry.vertices.resize(attributes.position->count, render::PbrVertex{});

        if (!fillAttribute(attributes.position, cgltf_type_vec3, 3, geometry.vertices,
                [](render::PbrVertex& vertex) { return vertex.position; }))
        {
            return primitiveError("primitive positions could not be read");
        }

        const bool hasNormals = fillAttribute(attributes.normal, cgltf_type_vec3, 3, geometry.vertices,
            [](render::PbrVertex& vertex) { return vertex.normal; });
        fillAttribute(attributes.uv, cgltf_type_vec2, 2, geometry.vertices,
            [](render::PbrVertex& vertex) { return vertex.uv; });
        const bool hasTangents = hasNormals && fillAttribute(attributes.tangent, cgltf_type_vec4, 4, geometry.vertices,
            [](render::PbrVertex& vertex) { return vertex.tangent; });

        geometry.indices = toTriangleList(primitive.type, readIndices(primitive, geometry.vertices.size()));

        for (const uint32_t index : geometry.indices)
        {
            if (index >= geometry.vertices.size())
            {
                return primitiveError("primitive index out of range");
            }
        }

        if (geometry.indices.empty())
        {
            return primitiveError("primitive has no triangles");
        }

        if (!hasNormals)
        {
            geometry.sourceVertices = geometry.indices;
            generateFlatNormals(geometry.vertices, geometry.indices);
        }

        if (!hasTangents)
        {
            generateTangents(geometry.vertices, geometry.indices);
        }

        for (const render::PbrVertex& vertex : geometry.vertices)
        {
            math::expand(geometry.bounds, math::Vec3{vertex.position[0], vertex.position[1], vertex.position[2]});
        }

        return geometry;
    }
} //namespace lockstep::assets
