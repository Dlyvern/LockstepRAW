#include "Presentation/MaterialShowcase.hpp"

#include <Lockstep/Core/Math.hpp>
#include <Lockstep/Core/Transform.hpp>
#include <Lockstep/Render/PbrMaterial.hpp>
#include <Lockstep/Render/Renderer.hpp>
#include <Lockstep/Render/Shapes.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace game
{
    namespace
    {
        namespace render = lockstep::render;
        namespace math = lockstep::math;

        constexpr uint32_t k_sphereRings{32};
        constexpr uint32_t k_sphereSegments{48};
        constexpr uint32_t k_roughnessSteps{7};
        constexpr float k_sphereRadius{0.45f};
        constexpr float k_rowSpacing{1.2f};
        constexpr float k_rowZ{-15.0f};
        constexpr float k_dielectricRowY{1.0f};
        constexpr float k_metalRowY{2.1f};
        constexpr uint32_t k_patternSize{128};
        constexpr uint32_t k_cutoutCell{16};
        constexpr float k_bumpFrequency{8.0f};
        constexpr float k_bumpStrength{0.6f};
        constexpr float k_cubeSpin{0.3f};
        constexpr math::Vec4 k_dielectricColour{0.8f, 0.08f, 0.06f, 1.0f};
        constexpr math::Vec4 k_goldColour{1.0f, 0.78f, 0.34f, 1.0f};

        render::PbrVertex vertex(const math::Vec3& position, const math::Vec3& normal, float u, float v,
            const math::Vec3& tangent)
        {
            return render::PbrVertex{
                .position = {position.x, position.y, position.z},
                .normal = {normal.x, normal.y, normal.z},
                .uv = {u, v},
                .tangent = {tangent.x, tangent.y, tangent.z, 1.0f}};
        }

        lockstep::Result<render::MeshHandle> createSphere(render::Renderer& renderer)
        {
            std::vector<render::PbrVertex> vertices;
            std::vector<uint32_t> indices;

            for (uint32_t ring = 0; ring <= k_sphereRings; ++ring)
            {
                const float v = static_cast<float>(ring) / static_cast<float>(k_sphereRings);
                const float polar = v * math::k_pi;

                for (uint32_t segment = 0; segment <= k_sphereSegments; ++segment)
                {
                    const float u = static_cast<float>(segment) / static_cast<float>(k_sphereSegments);
                    const float azimuth = u * 2.0f * math::k_pi;
                    const math::Vec3 normal{std::sin(polar) * std::cos(azimuth), std::cos(polar),
                        -std::sin(polar) * std::sin(azimuth)};
                    const math::Vec3 tangent{-std::sin(azimuth), 0.0f, -std::cos(azimuth)};
                    vertices.push_back(vertex(normal * 0.5f, normal, u * 2.0f, v, tangent));
                }
            }

            const uint32_t stride = k_sphereSegments + 1;

            for (uint32_t ring = 0; ring < k_sphereRings; ++ring)
            {
                for (uint32_t segment = 0; segment < k_sphereSegments; ++segment)
                {
                    const uint32_t topLeft = ring * stride + segment;
                    const uint32_t bottomLeft = topLeft + stride;
                    indices.insert(indices.end(), {topLeft, bottomLeft, topLeft + 1, topLeft + 1, bottomLeft,
                        bottomLeft + 1});
                }
            }

            return renderer.createMesh(std::span<const render::PbrVertex>(vertices), std::span<const uint32_t>(indices));
        }

        lockstep::Result<render::MeshHandle> createPanel(render::Renderer& renderer)
        {
            const math::Vec3 normal{0.0f, 0.0f, 1.0f};
            const math::Vec3 tangent{1.0f, 0.0f, 0.0f};
            const std::vector<render::PbrVertex> vertices{
                vertex({-0.5f, -0.5f, 0.0f}, normal, 0.0f, 1.0f, tangent),
                vertex({0.5f, -0.5f, 0.0f}, normal, 1.0f, 1.0f, tangent),
                vertex({0.5f, 0.5f, 0.0f}, normal, 1.0f, 0.0f, tangent),
                vertex({-0.5f, 0.5f, 0.0f}, normal, 0.0f, 0.0f, tangent)};
            const std::vector<uint32_t> indices{0, 1, 2, 2, 3, 0};

            return renderer.createMesh(std::span<const render::PbrVertex>(vertices), std::span<const uint32_t>(indices));
        }

        lockstep::Result<render::TextureHandle> createPattern(render::Renderer& renderer, render::TextureFormat format,
            auto&& texel)
        {
            std::vector<std::byte> pixels;
            pixels.reserve(static_cast<std::size_t>(k_patternSize) * k_patternSize * 4);

            for (uint32_t y = 0; y < k_patternSize; ++y)
            {
                for (uint32_t x = 0; x < k_patternSize; ++x)
                {
                    const std::array<uint8_t, 4> rgba = texel(x, y);
                    pixels.insert(pixels.end(), {std::byte{rgba[0]}, std::byte{rgba[1]}, std::byte{rgba[2]},
                        std::byte{rgba[3]}});
                }
            }

            return renderer.createTexture({.pixels = pixels, .width = k_patternSize, .height = k_patternSize,
                .format = format});
        }

        uint8_t toByte(float value)
        {
            return static_cast<uint8_t>(std::clamp(value * 0.5f + 0.5f, 0.0f, 1.0f) * 255.0f + 0.5f);
        }

        std::array<uint8_t, 4> bumpTexel(uint32_t x, uint32_t y)
        {
            const float scale = k_bumpFrequency * 2.0f * math::k_pi / static_cast<float>(k_patternSize);
            const float slopeX = std::cos(static_cast<float>(x) * scale) * std::sin(static_cast<float>(y) * scale);
            const float slopeY = std::sin(static_cast<float>(x) * scale) * std::cos(static_cast<float>(y) * scale);
            const math::Vec3 normal = math::normalize(math::Vec3{-slopeX * k_bumpStrength, -slopeY * k_bumpStrength, 1.0f});

            return {toByte(normal.x), toByte(normal.y), toByte(normal.z), 255};
        }

        std::array<uint8_t, 4> cutoutTexel(uint32_t x, uint32_t y)
        {
            const bool hole = ((x / k_cutoutCell) + (y / k_cutoutCell)) % 2 == 0;
            return {220, 225, 230, static_cast<uint8_t>(hole ? 0 : 255)};
        }

        lockstep::Status createRow(render::Renderer& renderer, const math::Vec4& colour,
            float metallic, std::vector<render::MaterialHandle>& row)
        {
            for (uint32_t step = 0; step < k_roughnessSteps; ++step)
            {
                const float roughness = static_cast<float>(step) / static_cast<float>(k_roughnessSteps - 1);
                auto material = renderer.createMaterial(render::PbrMaterialDescription{
                    .baseColorFactor = colour,
                    .metallicFactor = metallic,
                    .roughnessFactor = std::max(roughness, 0.05f)});

                if (!material)
                {
                    return material.error();
                }

                row.push_back(material.value());
            }

            return {};
        }
    } //namespace

    lockstep::Result<MaterialShowcase> createMaterialShowcase(render::Renderer& renderer)
    {
        MaterialShowcase showcase{};

        auto sphere = createSphere(renderer);

        if (!sphere)
        {
            return sphere.error();
        }

        const render::MeshData cubeData = render::shapes::cube();
        auto cube = renderer.createMesh(cubeData.vertices, cubeData.indices);

        if (!cube)
        {
            return cube.error();
        }

        auto panel = createPanel(renderer);

        if (!panel)
        {
            return panel.error();
        }

        showcase.sphere = sphere.value();
        showcase.cube = cube.value();
        showcase.panel = panel.value();

        if (auto row = createRow(renderer, k_dielectricColour, 0.0f, showcase.dielectrics); !row)
        {
            return row.error();
        }

        if (auto row = createRow(renderer, k_goldColour, 1.0f, showcase.metals); !row)
        {
            return row.error();
        }

        auto bumpMap = createPattern(renderer, render::TextureFormat::Rgba8Unorm, bumpTexel);

        if (!bumpMap)
        {
            return bumpMap.error();
        }

        auto cutoutMap = createPattern(renderer, render::TextureFormat::Rgba8Srgb, cutoutTexel);

        if (!cutoutMap)
        {
            return cutoutMap.error();
        }

        const render::PbrMaterialDescription descriptions[]{
            {.baseColorFactor = {0.55f, 0.55f, 0.6f, 1.0f}, .normal = bumpMap.value(), .metallicFactor = 0.0f,
                .roughnessFactor = 0.35f},
            {.baseColorFactor = {0.05f, 0.05f, 0.05f, 1.0f}, .metallicFactor = 0.0f, .roughnessFactor = 0.5f,
                .emissiveFactor = {4.0f, 1.6f, 0.4f}},
            {.baseColor = cutoutMap.value(), .metallicFactor = 0.0f, .roughnessFactor = 0.6f,
                .alphaMode = render::AlphaMode::Mask, .doubleSided = true},
            {.baseColorFactor = {0.6f, 0.85f, 1.0f, 0.3f}, .metallicFactor = 0.0f, .roughnessFactor = 0.05f,
                .alphaMode = render::AlphaMode::Blend, .doubleSided = true}};
        render::MaterialHandle* targets[]{&showcase.bumpy, &showcase.emissive, &showcase.cutout, &showcase.glass};

        for (std::size_t index = 0; index < std::size(descriptions); ++index)
        {
            auto material = renderer.createMaterial(descriptions[index]);

            if (!material)
            {
                return material.error();
            }

            *targets[index] = material.value();
        }

        return showcase;
    }

    void drawMaterialShowcase(render::Renderer& renderer, const MaterialShowcase& showcase, float seconds)
    {
        const float rowStart = -static_cast<float>(k_roughnessSteps - 1) * k_rowSpacing * 0.5f;
        const math::Vec3 sphereScale{k_sphereRadius * 2.0f, k_sphereRadius * 2.0f, k_sphereRadius * 2.0f};

        for (uint32_t step = 0; step < k_roughnessSteps; ++step)
        {
            const float x = rowStart + static_cast<float>(step) * k_rowSpacing;
            renderer.submit(showcase.sphere, showcase.dielectrics[step],
                {.position = {x, k_dielectricRowY, k_rowZ}, .scale = sphereScale});
            renderer.submit(showcase.sphere, showcase.metals[step],
                {.position = {x, k_metalRowY, k_rowZ}, .scale = sphereScale});
        }

        const float edge = -rowStart + k_rowSpacing * 1.5f;
        renderer.submit(showcase.cube, showcase.bumpy, {
            .position = {edge, 1.0f, k_rowZ},
            .rotation = math::angleAxis(seconds * k_cubeSpin, math::Vec3{0.0f, 1.0f, 0.0f}),
            .scale = {1.2f, 1.2f, 1.2f}});
        renderer.submit(showcase.sphere, showcase.emissive,
            {.position = {edge, 2.4f, k_rowZ}, .scale = {0.6f, 0.6f, 0.6f}});
        renderer.submit(showcase.panel, showcase.cutout,
            {.position = {-edge, 1.4f, k_rowZ}, .scale = {1.6f, 2.0f, 1.0f}});
        renderer.submit(showcase.panel, showcase.glass,
            {.position = {-edge - 1.0f, 1.4f, k_rowZ + 1.5f}, .scale = {1.6f, 2.0f, 1.0f}});
    }
} //namespace game
