#include "Gore/BloodAssets.hpp"

#include "Gore/BloodTextures.hpp"

#include <Lockstep/Assets/FileSystem.hpp>
#include <Lockstep/Physics/PhysicsWorld.hpp>
#include <Lockstep/Render/PbrMaterial.hpp>
#include <Lockstep/Render/Renderer.hpp>
#include <Lockstep/Render/Shapes.hpp>

#include <algorithm>
#include <array>
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
        constexpr std::string_view k_splatterNode{"Blood_Splatter"};
        constexpr std::string_view k_poolNode{"Blood_Pool"};
        constexpr std::array<std::string_view, k_runLooks> k_runNodes{"Blood_Run_1", "Blood_Run_2", "Blood_Run_3"};

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

        std::optional<render::MaterialHandle> nodeMaterial(const lockstep::assets::Scene& scene, std::string_view node)
        {
            for (const lockstep::assets::SceneInstance& instance : scene.instances)
            {
                if (instance.node < scene.nodes.size() && scene.nodes[instance.node].name == node
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

        lockstep::Result<render::MeshHandle> createRunQuad(render::Renderer& renderer, float share)
        {
            const float end = -0.5f + share;
            const std::vector<render::PbrVertex> vertices{
                {{-0.5f, 0.0f, -0.5f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f}},
                {{-0.5f, 0.0f, end}, {0.0f, 1.0f, 0.0f}, {0.0f, share}, {1.0f, 0.0f, 0.0f, 1.0f}},
                {{0.5f, 0.0f, end}, {0.0f, 1.0f, 0.0f}, {1.0f, share}, {1.0f, 0.0f, 0.0f, 1.0f}},
                {{0.5f, 0.0f, -0.5f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f}}};
            const std::vector<uint32_t> indices{0, 1, 2, 0, 2, 3};

            return renderer.createMesh(std::span<const render::PbrVertex>(vertices), std::span<const uint32_t>(indices));
        }

        lockstep::Result<bool> createRuns(render::Renderer& renderer, BloodAssets& assets)
        {
            if (!assets.art)
            {
                return false;
            }

            for (std::size_t look = 0; look < k_runLooks; ++look)
            {
                const std::optional<render::MaterialHandle> run = nodeMaterial(*assets.art, k_runNodes[look]);

                if (!run)
                {
                    return false;
                }

                assets.runs[look] = *run;
            }

            for (std::size_t step = 0; step < k_runSteps; ++step)
            {
                auto quad = createRunQuad(renderer, static_cast<float>(step + 1) / static_cast<float>(k_runSteps));

                if (!quad)
                {
                    return quad.error();
                }

                assets.runQuads[step] = quad.value();
            }

            return true;
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
        const std::optional<render::MaterialHandle> splat = assets.art ? nodeMaterial(*assets.art, k_bloodNode)
                                                                       : std::nullopt;
        const std::optional<render::MaterialHandle> splatter = assets.art
            ? nodeMaterial(*assets.art, k_splatterNode) : std::nullopt;
        const std::optional<render::MaterialHandle> artPool = assets.art ? nodeMaterial(*assets.art, k_poolNode)
                                                                         : std::nullopt;

        if (auto runs = createRuns(renderer, assets))
        {
            assets.hasRuns = runs.value();
        }
        else
        {
            return runs.error();
        }
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
            assets.splatters[index] = index > 0 && splatter ? *splatter : *splat;
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

        const std::optional<render::MaterialHandle> poolArt = artPool ? artPool : splat;
        auto pool = poolArt ? lockstep::Result<render::MaterialHandle>(render::MaterialHandle{*poolArt})
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

    void layWallRun(BloodDecals& decals, const BloodAssets& assets, const lockstep::physics::PhysicsWorld& physics,
        const lockstep::physics::RaycastHit& hit, float size, const WallRunTuning& tuning, uint32_t maxDecals,
        GoreRandom& random)
    {
        if (!assets.hasRuns || !isRunningSurface(hit.normal) || random.next() >= tuning.chance)
        {
            return;
        }

        const auto look = std::min(static_cast<std::size_t>(random.next() * static_cast<float>(k_runLooks)),
            k_runLooks - 1);
        addWallRun(decals, physics, assets.runs[look], hit.point, hit.normal,
            size * tuning.lengthShare * random.range(0.7f, 1.2f), random.range(tuning.minSeconds, tuning.maxSeconds),
            maxDecals);
    }
} //namespace game
