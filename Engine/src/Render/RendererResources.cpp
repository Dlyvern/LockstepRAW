#include "Lockstep/Render/Renderer.hpp"

#include "RendererState.hpp"
#include "VelaConversions.hpp"

#include <Vela/Graphics/ImageData.hpp>
#include <Vela/Graphics/Vertex.hpp>
#include <Vela/Graphics/VertexLayout.hpp>

#include <cstddef>
#include <cstdint>
#include <string>

namespace lockstep::render
{
    namespace
    {
        constexpr std::size_t k_bytesPerPixel{4};

        static_assert(sizeof(Vertex) == sizeof(vela::graphics::StaticVertex));
        static_assert(offsetof(Vertex, position) == offsetof(vela::graphics::StaticVertex, position));
        static_assert(offsetof(Vertex, uv) == offsetof(vela::graphics::StaticVertex, uv));
        static_assert(offsetof(Vertex, normal) == offsetof(vela::graphics::StaticVertex, normal));

        constexpr uint32_t k_albedoSlot{0};
        constexpr uint32_t k_lightingSlot{1};

        vela::graphics::VertexLayout unlitVertexLayout()
        {
            return vela::graphics::VertexLayout
            {
                sizeof(Vertex),
                {
                    {0, vela::graphics::VertexAttributeFormat::Float3, static_cast<uint32_t>(offsetof(Vertex, position))},
                    {1, vela::graphics::VertexAttributeFormat::Float2, static_cast<uint32_t>(offsetof(Vertex, uv))}
                }
            };
        }

        vela::graphics::VertexLayout litVertexLayout()
        {
            return vela::graphics::VertexLayout
            {
                sizeof(Vertex),
                {
                    {0, vela::graphics::VertexAttributeFormat::Float3, static_cast<uint32_t>(offsetof(Vertex, position))},
                    {1, vela::graphics::VertexAttributeFormat::Float2, static_cast<uint32_t>(offsetof(Vertex, uv))},
                    {2, vela::graphics::VertexAttributeFormat::Float3, static_cast<uint32_t>(offsetof(Vertex, normal))}
                }
            };
        }
    } //namespace

    Result<MeshHandle> Renderer::createMesh(std::span<const Vertex> vertices, std::span<const uint32_t> indices)
    {
        if (vertices.empty() || indices.empty())
        {
            return Error{ErrorCode::InvalidArgument, "createMesh: vertices and indices must not be empty"};
        }

        auto mesh = vela::graphics::Mesh::create(m_state->context, std::as_bytes(vertices),
            static_cast<uint32_t>(vertices.size()), indices);

        if (!mesh)
        {
            return toError(mesh.error());
        }

        return m_state->meshes.add(std::move(mesh).value());
    }

    Result<TextureHandle> Renderer::createTexture(const TextureDescription& description)
    {
        const std::size_t expectedSize = static_cast<std::size_t>(description.width) * description.height
            * k_bytesPerPixel;

        if (description.width == 0 || description.height == 0 || description.pixels.size() != expectedSize)
        {
            return Error{ErrorCode::InvalidArgument, "createTexture: expected " + std::to_string(expectedSize)
                + " bytes of pixel data, got " + std::to_string(description.pixels.size())};
        }

        vela::graphics::ImageData image{};
        image.pixels = description.pixels;
        image.width = description.width;
        image.height = description.height;
        image.format = toVelaFormat(description.format);

        auto texture = vela::graphics::Texture::create(m_state->context, image, toVelaSampler(description.filter));

        if (!texture)
        {
            return toError(texture.error());
        }

        return m_state->textures.add(std::move(texture).value());
    }

    Result<MaterialHandle> Renderer::createMaterial(const MaterialDescription& description)
    {
        const vela::graphics::Texture* albedo = m_state->textures.find(description.albedo);

        if (albedo == nullptr)
        {
            return Error{ErrorCode::InvalidHandle, "createMaterial: albedo texture handle is invalid"};
        }

        const bool isLit = description.shading == Shading::Lit;
        const MaterialShaders& shaders = m_state->shaders;

        auto material = vela::graphics::Material::create(m_state->context,
        {
            .vertexShader = isLit ? shaders.litVertex : shaders.unlitVertex,
            .fragmentShader = isLit ? shaders.litFragment : shaders.unlitFragment,
            .vertexLayout = isLit ? litVertexLayout() : unlitVertexLayout()
        });

        if (!material)
        {
            return toError(material.error());
        }

        if (auto bound = material.value().setTexture(k_albedoSlot, *albedo); !bound)
        {
            return toError(bound.error());
        }

        if (isLit)
        {
            if (auto bound = material.value().setStorageBuffer(k_lightingSlot, m_state->lightBuffer); !bound)
            {
                return toError(bound.error());
            }
        }

        return m_state->materials.add(MaterialRecord{
            .material = std::move(material).value(),
            .shading = description.shading,
            .surface = {description.specular, description.shininess, description.emissive, 0.0f}});
    }
} //namespace lockstep::render
