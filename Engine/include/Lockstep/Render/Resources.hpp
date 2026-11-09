#ifndef LOCKSTEP_RENDER_RESOURCES_HPP
#define LOCKSTEP_RENDER_RESOURCES_HPP

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace lockstep::render
{
    // Typed index of a GPU resource owned by the Renderer. Cheap to copy and safe to store in game
    // state. The Tag only keeps handles of different resource kinds from mixing.
    template<typename Tag>
    struct Handle
    {
        static constexpr uint32_t k_invalidIndex{0xFFFFFFFFu};

        uint32_t index{k_invalidIndex};

        // Returns: true if the handle refers to a resource (it may still belong to another Renderer).
        [[nodiscard]] constexpr bool isValid() const noexcept
        {
            return index != k_invalidIndex;
        }

        friend constexpr bool operator==(Handle, Handle) noexcept = default;
    };

    using MeshHandle = Handle<struct MeshTag>;
    using TextureHandle = Handle<struct TextureTag>;
    using MaterialHandle = Handle<struct MaterialTag>;

    // One vertex of a static mesh: position, texture coordinate and normal.
    struct Vertex
    {
        float position[3];
        float uv[2];
        float normal[3];
    };

    // CPU-side mesh data, ready to be passed to Renderer::createMesh.
    struct MeshData
    {
        std::vector<Vertex> vertices;
        std::vector<uint32_t> indices;
    };

    // Pixel layout of texture data. Both are 4 bytes per pixel.
    enum class TextureFormat : uint8_t
    {
        Rgba8Srgb = 0,
        Rgba8Unorm
    };

    // How a texture is sampled when magnified or minified.
    enum class TextureFilter : uint8_t
    {
        Linear = 0,
        Nearest
    };

    // Everything needed to create a 2D texture. `pixels` is only read during creation.
    struct TextureDescription
    {
        std::span<const std::byte> pixels;
        uint32_t width{0};
        uint32_t height{0};
        TextureFormat format{TextureFormat::Rgba8Srgb};
        TextureFilter filter{TextureFilter::Linear};
    };

    // How a material responds to light.
    // Lit:   diffuse + specular from the frame's lights, ambient, shadows and fog. For the world.
    // Unlit: shows the albedo as is; no lights, shadows or fog, and casts no shadow. For HUD
    //        elements and debug shapes.
    enum class Shading : uint8_t
    {
        Lit = 0,
        Unlit
    };

    // Everything needed to create a textured material.
    // albedo:    the base colour texture.
    // shading:   lit or unlit.
    // specular:  strength of specular highlights (lit only), 0 for matte.
    // shininess: Blinn-Phong exponent (lit only); higher is a tighter, glossier highlight.
    // emissive:  how much of the albedo glows by itself regardless of lights (lit only), 0 for none.
    struct MaterialDescription
    {
        TextureHandle albedo{};
        Shading shading{Shading::Lit};
        float specular{0.2f};
        float shininess{24.0f};
        float emissive{0.0f};
    };
} //namespace lockstep::render

#endif //LOCKSTEP_RENDER_RESOURCES_HPP
