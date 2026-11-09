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

    // Pixel layout of texture data. Rgba8 formats are 4 bytes per pixel. Bc formats are block
    // compressed in 4x4 pixel blocks (a partial block at the edge still takes a whole block):
    // Bc1 8 bytes per block (RGB, 1-bit alpha), Bc3 and Bc7 16 bytes per block (RGBA).
    // Srgb formats are decoded from sRGB to linear when sampled; use them for colour data only.
    enum class TextureFormat : uint8_t
    {
        Rgba8Srgb = 0,
        Rgba8Unorm,
        Bc1Srgb,
        Bc1Unorm,
        Bc3Srgb,
        Bc3Unorm,
        Bc7Srgb,
        Bc7Unorm
    };

    // How a texture is sampled when magnified or minified.
    enum class TextureFilter : uint8_t
    {
        Linear = 0,
        Nearest
    };

    // Everything needed to create a 2D texture. `pixels` is only read during creation.
    // pixels: every mip level, largest first, tightly packed. Level n is max(1, width >> n) by
    //         max(1, height >> n) pixels; its size is textureLevelBytes() of that extent.
    // levels: number of mip levels in `pixels`, 1 for none. At most the full chain down to 1x1.
    struct TextureDescription
    {
        std::span<const std::byte> pixels;
        uint32_t width{0};
        uint32_t height{0};
        TextureFormat format{TextureFormat::Rgba8Srgb};
        TextureFilter filter{TextureFilter::Linear};
        uint32_t levels{1};
    };

    // format:  pixel layout.
    // width:   level width in pixels.
    // height:  level height in pixels.
    // Returns: bytes of one mip level of that size, block-rounded for Bc formats.
    [[nodiscard]] std::size_t textureLevelBytes(TextureFormat format, uint32_t width, uint32_t height) noexcept;

    // width:   base level width in pixels.
    // height:  base level height in pixels.
    // Returns: number of levels in a full mip chain down to 1x1 (1 for a 1x1 texture).
    [[nodiscard]] uint32_t fullMipLevelCount(uint32_t width, uint32_t height) noexcept;

    // How a material responds to light.
    // Lit:   PBR shading from the frame's lights, the sky, shadows and fog. For the world.
    // Unlit: shows the albedo as is; no lights, shadows or fog, and casts no shadow. For HUD
    //        elements and debug shapes.
    enum class Shading : uint8_t
    {
        Lit = 0,
        Unlit
    };

    // Everything needed to create a simple textured material. Lit ones become a double-sided
    // dielectric PbrMaterialDescription (see PbrMaterial.hpp for the full model).
    // albedo:    the base colour texture.
    // shading:   lit or unlit.
    // specular:  unused since lit materials are PBR (kept for source compatibility).
    // shininess: glossiness (lit only); mapped to PBR roughness sqrt(2 / (shininess + 2)).
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
