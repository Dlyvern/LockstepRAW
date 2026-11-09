#include "Lockstep/Render/Renderer.hpp"

#include "RendererState.hpp"
#include "VelaConversions.hpp"

#include <Vela/Graphics/VertexLayout.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace lockstep::render
{
    namespace
    {
        enum class PbrSlot : uint32_t
        {
            BaseColor = 0,
            Normal,
            MetallicRoughness,
            Occlusion,
            Emissive,
            Lighting
        };

        constexpr uint32_t k_unlitAlbedoSlot{0};
        constexpr uint32_t k_maskedShadowBaseColorSlot{0};
        constexpr float k_notMasked{-1.0f};

        vela::graphics::VertexLayout pbrVertexLayout()
        {
            return vela::graphics::VertexLayout
            {
                sizeof(PbrVertex),
                {
                    {0, vela::graphics::VertexAttributeFormat::Float3, static_cast<uint32_t>(offsetof(PbrVertex, position))},
                    {1, vela::graphics::VertexAttributeFormat::Float3, static_cast<uint32_t>(offsetof(PbrVertex, normal))},
                    {2, vela::graphics::VertexAttributeFormat::Float2, static_cast<uint32_t>(offsetof(PbrVertex, uv))},
                    {3, vela::graphics::VertexAttributeFormat::Float4, static_cast<uint32_t>(offsetof(PbrVertex, tangent))}
                }
            };
        }

        vela::graphics::VertexLayout positionUvLayout()
        {
            return vela::graphics::VertexLayout
            {
                sizeof(PbrVertex),
                {
                    {0, vela::graphics::VertexAttributeFormat::Float3, static_cast<uint32_t>(offsetof(PbrVertex, position))},
                    {1, vela::graphics::VertexAttributeFormat::Float2, static_cast<uint32_t>(offsetof(PbrVertex, uv))}
                }
            };
        }

        math::Vec3 toVec3(const float (&values)[3])
        {
            return {values[0], values[1], values[2]};
        }

        std::vector<PbrVertex> toPbrVertices(std::span<const Vertex> vertices)
        {
            std::vector<PbrVertex> converted;
            converted.reserve(vertices.size());

            for (const Vertex& vertex : vertices)
            {
                converted.push_back(PbrVertex{
                    .position = {vertex.position[0], vertex.position[1], vertex.position[2]},
                    .normal = {vertex.normal[0], vertex.normal[1], vertex.normal[2]},
                    .uv = {vertex.uv[0], vertex.uv[1]},
                    .tangent = {0.0f, 0.0f, 0.0f, 1.0f}});
            }

            return converted;
        }

        std::vector<uint32_t> counterClockwiseIndices(std::span<const PbrVertex> vertices, std::span<const uint32_t> indices)
        {
            std::vector<uint32_t> ordered(indices.begin(), indices.end());

            for (std::size_t first = 0; first + 2 < ordered.size(); first += 3)
            {
                const PbrVertex& a = vertices[ordered[first]];
                const PbrVertex& b = vertices[ordered[first + 1]];
                const PbrVertex& c = vertices[ordered[first + 2]];
                const math::Vec3 faceNormal = math::cross(toVec3(b.position) - toVec3(a.position),
                    toVec3(c.position) - toVec3(a.position));

                if (math::dot(faceNormal, toVec3(a.normal)) < 0.0f)
                {
                    std::swap(ordered[first + 1], ordered[first + 2]);
                }
            }

            return ordered;
        }

        void generateTangents(std::vector<PbrVertex>& vertices, std::span<const uint32_t> indices)
        {
            std::vector<math::Vec3> tangents(vertices.size(), math::Vec3{0.0f, 0.0f, 0.0f});
            std::vector<math::Vec3> bitangents(vertices.size(), math::Vec3{0.0f, 0.0f, 0.0f});

            for (std::size_t first = 0; first + 2 < indices.size(); first += 3)
            {
                const std::array<uint32_t, 3> corner{indices[first], indices[first + 1], indices[first + 2]};
                const PbrVertex& a = vertices[corner[0]];
                const PbrVertex& b = vertices[corner[1]];
                const PbrVertex& c = vertices[corner[2]];
                const math::Vec3 edge1 = toVec3(b.position) - toVec3(a.position);
                const math::Vec3 edge2 = toVec3(c.position) - toVec3(a.position);
                const float du1 = b.uv[0] - a.uv[0];
                const float dv1 = b.uv[1] - a.uv[1];
                const float du2 = c.uv[0] - a.uv[0];
                const float dv2 = c.uv[1] - a.uv[1];
                const float determinant = du1 * dv2 - du2 * dv1;

                if (std::abs(determinant) < 1e-12f)
                {
                    continue;
                }

                const float inverse = 1.0f / determinant;
                const math::Vec3 tangent = (edge1 * dv2 - edge2 * dv1) * inverse;
                const math::Vec3 bitangent = (edge2 * du1 - edge1 * du2) * inverse;

                for (const uint32_t index : corner)
                {
                    tangents[index] += tangent;
                    bitangents[index] += bitangent;
                }
            }

            for (std::size_t index = 0; index < vertices.size(); ++index)
            {
                const math::Vec3 normal = toVec3(vertices[index].normal);
                const math::Vec3 orthogonal = tangents[index] - normal * math::dot(normal, tangents[index]);

                if (math::lengthSquared(orthogonal) < 1e-12f)
                {
                    continue;
                }

                const math::Vec3 tangent = math::normalize(orthogonal);
                const float handedness = math::dot(math::cross(normal, tangent), bitangents[index]) < 0.0f ? 1.0f : -1.0f;
                vertices[index].tangent[0] = tangent.x;
                vertices[index].tangent[1] = tangent.y;
                vertices[index].tangent[2] = tangent.z;
                vertices[index].tangent[3] = handedness;
            }
        }

        math::Aabb meshBounds(std::span<const PbrVertex> vertices)
        {
            math::Aabb bounds{};

            for (const PbrVertex& vertex : vertices)
            {
                math::expand(bounds, toVec3(vertex.position));
            }

            return bounds;
        }

        float roughnessFromShininess(float shininess)
        {
            return std::clamp(std::sqrt(2.0f / (std::max(shininess, 0.0f) + 2.0f)), 0.05f, 1.0f);
        }

        Result<const vela::graphics::Texture*> resolveTexture(const RendererState& state, TextureHandle handle,
            const vela::graphics::Texture& fallback, const char* name)
        {
            if (!handle.isValid())
            {
                return &fallback;
            }

            const vela::graphics::Texture* texture = state.textures.find(handle);

            if (texture == nullptr)
            {
                return Error{ErrorCode::InvalidHandle, std::string("createMaterial: ") + name + " texture handle is invalid"};
            }

            return texture;
        }

        Status bindTexture(vela::graphics::Material& material, PbrSlot slot, const vela::graphics::Texture& texture)
        {
            if (auto bound = material.setTexture(slot, texture); !bound)
            {
                return toError(bound.error());
            }

            return {};
        }

        Result<vela::graphics::Material> createMaskedShadowMaterial(RendererState& state,
            const vela::graphics::Texture& baseColor)
        {
            auto material = vela::graphics::Material::create(state.context,
            {
                .vertexShader = state.shaders.shadowMaskVertex,
                .fragmentShader = state.shaders.shadowMaskFragment,
                .vertexLayout = positionUvLayout()
            });

            if (!material)
            {
                return toError(material.error());
            }

            if (auto bound = material.value().setTexture(k_maskedShadowBaseColorSlot, baseColor); !bound)
            {
                return toError(bound.error());
            }

            return std::move(material).value();
        }

        PbrFactors toFactors(const PbrMaterialDescription& description)
        {
            PbrFactors factors{};
            const math::Vec4& base = description.baseColorFactor;
            const math::Vec3& emissive = description.emissiveFactor;

            factors.baseColorFactor[0] = base.x;
            factors.baseColorFactor[1] = base.y;
            factors.baseColorFactor[2] = base.z;
            factors.baseColorFactor[3] = base.w;
            factors.emissiveNormalScale[0] = emissive.x;
            factors.emissiveNormalScale[1] = emissive.y;
            factors.emissiveNormalScale[2] = emissive.z;
            factors.emissiveNormalScale[3] = description.normalScale;
            factors.surface[0] = description.metallicFactor;
            factors.surface[1] = description.roughnessFactor;
            factors.surface[2] = description.occlusionStrength;
            factors.surface[3] = description.alphaMode == AlphaMode::Mask ? description.alphaCutoff : k_notMasked;
            factors.flags[0] = description.doubleSided ? 1.0f : 0.0f;
            factors.flags[1] = description.alphaMode == AlphaMode::Blend ? 1.0f : 0.0f;

            return factors;
        }
    } //namespace

    Result<MeshHandle> Renderer::createMesh(std::span<const Vertex> vertices, std::span<const uint32_t> indices)
    {
        if (vertices.empty() || indices.empty())
        {
            return Error{ErrorCode::InvalidArgument, "createMesh: vertices and indices must not be empty"};
        }

        std::vector<PbrVertex> converted = toPbrVertices(vertices);
        const std::vector<uint32_t> ordered = counterClockwiseIndices(converted, indices);
        generateTangents(converted, ordered);

        return createMesh(std::span<const PbrVertex>(converted), std::span<const uint32_t>(ordered));
    }

    Result<MeshHandle> Renderer::createMesh(std::span<const PbrVertex> vertices, std::span<const uint32_t> indices)
    {
        if (vertices.empty() || indices.empty())
        {
            return Error{ErrorCode::InvalidArgument, "createMesh: vertices and indices must not be empty"};
        }

        const auto outOfRange = std::find_if(indices.begin(), indices.end(),
            [&vertices](uint32_t index) { return index >= vertices.size(); });

        if (outOfRange != indices.end())
        {
            return Error{ErrorCode::InvalidArgument, "createMesh: index out of range"};
        }

        auto mesh = vela::graphics::Mesh::create(m_state->context, std::as_bytes(vertices),
            static_cast<uint32_t>(vertices.size()), indices);

        if (!mesh)
        {
            return toError(mesh.error());
        }

        return m_state->meshes.add(MeshRecord{.mesh = std::move(mesh).value(), .bounds = meshBounds(vertices)});
    }

    Result<MaterialHandle> Renderer::createMaterial(const MaterialDescription& description)
    {
        const vela::graphics::Texture* albedo = m_state->textures.find(description.albedo);

        if (albedo == nullptr)
        {
            return Error{ErrorCode::InvalidHandle, "createMaterial: albedo texture handle is invalid"};
        }

        if (description.shading == Shading::Lit)
        {
            PbrMaterialDescription pbr{};
            pbr.baseColor = description.albedo;
            pbr.metallicFactor = 0.0f;
            pbr.roughnessFactor = roughnessFromShininess(description.shininess);
            pbr.emissive = description.albedo;
            pbr.emissiveFactor = {description.emissive, description.emissive, description.emissive};
            pbr.doubleSided = true;

            return createMaterial(pbr);
        }

        auto material = vela::graphics::Material::create(m_state->context,
        {
            .vertexShader = m_state->shaders.unlitVertex,
            .fragmentShader = m_state->shaders.unlitFragment,
            .vertexLayout = positionUvLayout()
        });

        if (!material)
        {
            return toError(material.error());
        }

        if (auto bound = material.value().setTexture(k_unlitAlbedoSlot, *albedo); !bound)
        {
            return toError(bound.error());
        }

        return m_state->materials.add(MaterialRecord{
            .material = std::move(material).value(),
            .maskedShadow = std::nullopt,
            .shading = Shading::Unlit,
            .alphaMode = AlphaMode::Opaque,
            .factors = {}});
    }

    Result<MaterialHandle> Renderer::createMaterial(const PbrMaterialDescription& description)
    {
        RendererState& state = *m_state;
        const EnvironmentResources& defaults = state.environment;

        const std::array<std::tuple<PbrSlot, TextureHandle, const vela::graphics::Texture*, const char*>, 5> textures{{
            {PbrSlot::BaseColor, description.baseColor, &defaults.whiteSrgb, "base colour"},
            {PbrSlot::Normal, description.normal, &defaults.flatNormal, "normal"},
            {PbrSlot::MetallicRoughness, description.metallicRoughness, &defaults.whiteLinear, "metallic-roughness"},
            {PbrSlot::Occlusion, description.occlusion, &defaults.whiteLinear, "occlusion"},
            {PbrSlot::Emissive, description.emissive, &defaults.whiteSrgb, "emissive"}}};

        std::array<const vela::graphics::Texture*, 5> resolved{};

        for (std::size_t index = 0; index < textures.size(); ++index)
        {
            const auto& [slot, handle, fallback, name] = textures[index];
            auto texture = resolveTexture(state, handle, *fallback, name);

            if (!texture)
            {
                return texture.error();
            }

            resolved[index] = texture.value();
        }

        const bool isBlended = description.alphaMode == AlphaMode::Blend;
        vela::graphics::RenderState renderState{};
        renderState.cull = description.doubleSided ? vela::graphics::CullMode::None : vela::graphics::CullMode::Back;
        renderState.frontFace = vela::graphics::FrontFace::CounterClockwise;
        renderState.depthWrite = !isBlended;
        renderState.blend = isBlended ? vela::graphics::BlendMode::Alpha : vela::graphics::BlendMode::Opaque;

        auto material = vela::graphics::Material::create(state.context,
        {
            .vertexShader = state.shaders.pbrVertex,
            .fragmentShader = state.shaders.pbrFragment,
            .vertexLayout = pbrVertexLayout(),
            .renderState = renderState
        });

        if (!material)
        {
            return toError(material.error());
        }

        for (std::size_t index = 0; index < textures.size(); ++index)
        {
            if (auto bound = bindTexture(material.value(), std::get<0>(textures[index]), *resolved[index]); !bound)
            {
                return bound.error();
            }
        }

        if (auto bound = material.value().setStorageBuffer(static_cast<uint32_t>(PbrSlot::Lighting), state.lightBuffer);
            !bound)
        {
            return toError(bound.error());
        }

        MaterialRecord record{
            .material = std::move(material).value(),
            .maskedShadow = std::nullopt,
            .shading = Shading::Lit,
            .alphaMode = description.alphaMode,
            .factors = toFactors(description)};

        if (description.alphaMode == AlphaMode::Mask)
        {
            auto shadow = createMaskedShadowMaterial(state, *resolved[0]);

            if (!shadow)
            {
                return shadow.error();
            }

            record.maskedShadow.emplace(std::move(shadow).value());
        }

        return state.materials.add(std::move(record));
    }
} //namespace lockstep::render
