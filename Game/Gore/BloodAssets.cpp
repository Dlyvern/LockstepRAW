#include "Gore/BloodAssets.hpp"

#include "Gore/BloodTextures.hpp"

#include <Lockstep/Assets/FileSystem.hpp>
#include <Lockstep/Render/PbrMaterial.hpp>
#include <Lockstep/Render/Renderer.hpp>
#include <Lockstep/Render/Shapes.hpp>

#include <cstdint>
#include <iostream>
#include <string_view>
#include <span>
#include <vector>

namespace game
{
    namespace
    {
        namespace render = lockstep::render;

        constexpr uint32_t k_largeSize{512};
        constexpr uint32_t k_smallSize{256};
        constexpr uint32_t k_splatterSeed{1031};
        constexpr uint32_t k_poolSeed{2203};
        constexpr uint32_t k_dropSeed{3301};
        constexpr float k_wetRoughness{0.18f};
        constexpr float k_poolRoughness{0.08f};
        constexpr lockstep::math::Vec4 k_dropletColour{0.16f, 0.004f, 0.003f, 1.0f};
        constexpr std::string_view k_goreScene{"characters/gore/scene"};
        constexpr std::string_view k_bloodNode{"Blood_Splat"};

        std::optional<lockstep::assets::Scene> loadArt(render::Renderer& renderer,
            const lockstep::assets::FileSystem& files)
        {
            if (!files.exists(k_goreScene))
            {
                std::clog << "Game: blood art '" << k_goreScene << "' not mounted; blood uses generated textures\n";
                return std::nullopt;
            }

            auto loaded = lockstep::assets::loadCookedScene(renderer, files, k_goreScene);

            if (!loaded)
            {
                std::clog << "Game: blood art not loaded (" << loaded.error().message << "); blood uses generated textures\n";
                return std::nullopt;
            }

            return std::move(loaded).value();
        }

        std::optional<render::MaterialHandle> splatMaterial(const lockstep::assets::Scene& scene)
        {
            for (const lockstep::assets::SceneInstance& instance : scene.instances)
            {
                if (instance.node < scene.nodes.size() && scene.nodes[instance.node].name == k_bloodNode
                    && instance.mesh < scene.meshes.size())
                {
                    return scene.meshes[instance.mesh].material;
                }
            }

            return std::nullopt;
        }

        lockstep::Result<render::MaterialHandle> bloodMaterial(render::Renderer& renderer, BloodTextureKind kind,
            uint32_t size, uint32_t seed, float roughness)
        {
            const std::vector<std::byte> pixels = bloodTexture(kind, size, seed);
            auto texture = renderer.createTexture(render::TextureDescription{.pixels = pixels,
                .width = size,
                .height = size,
                .format = render::TextureFormat::Rgba8Srgb,
                .generateMips = true});

            if (!texture)
            {
                return texture.error();
            }

            return renderer.createMaterial(render::PbrMaterialDescription{.baseColor = texture.value(),
                .metallicFactor = 0.0f,
                .roughnessFactor = roughness,
                .alphaMode = render::AlphaMode::Blend,
                .doubleSided = true});
        }

        lockstep::Result<render::MeshHandle> createDecalQuad(render::Renderer& renderer)
        {
            const std::vector<render::PbrVertex> vertices{
                {{-0.5f, 0.0f, -0.5f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f}},
                {{-0.5f, 0.0f, 0.5f}, {0.0f, 1.0f, 0.0f}, {0.0f, 1.0f}, {1.0f, 0.0f, 0.0f, 1.0f}},
                {{0.5f, 0.0f, 0.5f}, {0.0f, 1.0f, 0.0f}, {1.0f, 1.0f}, {1.0f, 0.0f, 0.0f, 1.0f}},
                {{0.5f, 0.0f, -0.5f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f}}};
            const std::vector<uint32_t> indices{0, 1, 2, 0, 2, 3};

            return renderer.createMesh(std::span<const render::PbrVertex>(vertices), std::span<const uint32_t>(indices));
        }
    } //namespace

    lockstep::Result<BloodAssets> createBloodAssets(render::Renderer& renderer, const lockstep::assets::FileSystem& files)
    {
        BloodAssets assets{};
        assets.art = loadArt(renderer, files);
        const std::optional<render::MaterialHandle> splat = assets.art ? splatMaterial(*assets.art) : std::nullopt;
        auto quad = createDecalQuad(renderer);

        if (!quad)
        {
            return quad.error();
        }

        assets.decalQuad = quad.value();
        const render::MeshData cube = render::shapes::cube();
        auto droplet = renderer.createMesh(std::span<const render::Vertex>(cube.vertices),
            std::span<const uint32_t>(cube.indices));

        if (!droplet)
        {
            return droplet.error();
        }

        assets.droplet = droplet.value();

        for (std::size_t index = 0; index < assets.splatters.size() && splat; ++index)
        {
            assets.splatters[index] = *splat;
        }

        for (std::size_t index = 0; index < assets.splatters.size() && !splat; ++index)
        {
            auto splatter = bloodMaterial(renderer, BloodTextureKind::Splatter, k_largeSize,
                k_splatterSeed + static_cast<uint32_t>(index) * 97u, k_wetRoughness);

            if (!splatter)
            {
                return splatter.error();
            }

            assets.splatters[index] = splatter.value();
        }

        auto pool = splat ? lockstep::Result<render::MaterialHandle>(render::MaterialHandle{*splat})
                          : bloodMaterial(renderer, BloodTextureKind::Pool, k_largeSize, k_poolSeed, k_poolRoughness);

        if (!pool)
        {
            return pool.error();
        }

        assets.pool = pool.value();
        auto drop = bloodMaterial(renderer, BloodTextureKind::Drop, k_smallSize, k_dropSeed, k_wetRoughness);

        if (!drop)
        {
            return drop.error();
        }

        assets.drop = drop.value();
        auto dropletLook = renderer.createMaterial(render::PbrMaterialDescription{.baseColorFactor = k_dropletColour,
            .metallicFactor = 0.0f,
            .roughnessFactor = k_wetRoughness});

        if (!dropletLook)
        {
            return dropletLook.error();
        }

        assets.dropletLook = dropletLook.value();

        return assets;
    }
} //namespace game
