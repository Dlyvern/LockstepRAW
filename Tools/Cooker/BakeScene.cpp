#include "BakeScene.hpp"

#include <Lockstep/Assets/SceneLights.hpp>
#include <Lockstep/Core/Transform.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <string_view>

namespace lockstep::cooker
{
    namespace
    {
        constexpr std::array<std::string_view, 6> k_skippedNodePrefixes{"Interact_", "Breakable_Glass", "Toy_",
            "Decal_", "Npc_", "Spawn_"};
        constexpr std::array<std::string_view, 4> k_unbouncedLightTags{"_Flicker", "_Siren", "_Pulse", "_NoBounce"};
        constexpr std::string_view k_tubeTag{"_Tube"};
        constexpr uint32_t k_tubePoints{4};
        constexpr float k_maxAlbedo{0.95f};
        constexpr float k_emissiveThreshold{1.0e-4f};
        constexpr float k_surfaceOffset{0.02f};
        constexpr float k_lightClearance{0.05f};
        constexpr float k_minFalloffDistanceSquared{0.01f};
        constexpr float k_minContribution{1.0e-6f};
        constexpr uint32_t k_skyRays{8};
        constexpr float k_lightCellSize{2.0f};
        constexpr int32_t k_maxLightCells{64};
        constexpr float k_infinity{1.0e30f};

        uint64_t mixBits(uint64_t value)
        {
            value ^= value >> 30;
            value *= 0xBF58476D1CE4E5B9ull;
            value ^= value >> 27;
            value *= 0x94D049BB133111EBull;
            value ^= value >> 31;
            return value;
        }

        float unitFloat(uint64_t bits)
        {
            return static_cast<float>(bits >> 40) * (1.0f / 16777216.0f);
        }

        float smoothstep(float edge0, float edge1, float value)
        {
            if (!(edge1 > edge0))
            {
                return value >= edge1 ? 1.0f : 0.0f;
            }

            const float t = std::clamp((value - edge0) / (edge1 - edge0), 0.0f, 1.0f);
            return t * t * (3.0f - 2.0f * t);
        }

        bool isSkippedNode(const SourceScene& scene, uint32_t node)
        {
            while (node < scene.nodes.size())
            {
                const std::string_view name = scene.nodes[node].name;

                for (const std::string_view prefix : k_skippedNodePrefixes)
                {
                    if (name.starts_with(prefix))
                    {
                        return true;
                    }
                }

                node = scene.nodes[node].parent;
            }

            return false;
        }

        TextureMean meanOf(std::span<const TextureMean> means, uint32_t texture)
        {
            return texture < means.size() ? means[texture] : TextureMean{};
        }

        BakeSurface surfaceOf(const assets::CookedMaterial& material, std::span<const TextureMean> means,
            RaySurface& raySurface)
        {
            const render::PbrMaterialDescription& factors = material.factors;
            const TextureMean base = meanOf(means, material.baseColor);
            const float metallic = std::clamp(factors.metallicFactor
                * (material.metallicRoughness != assets::k_noTexture ? meanOf(means, material.metallicRoughness).colour.z
                    : 1.0f), 0.0f, 1.0f);
            const math::Vec3 baseColour{factors.baseColorFactor.x, factors.baseColorFactor.y, factors.baseColorFactor.z};
            const math::Vec3 albedo = baseColour * base.colour * (1.0f - metallic);
            const math::Vec3 emissive = factors.emissiveFactor
                * (material.emissive != assets::k_noTexture ? meanOf(means, material.emissive).colour
                    : math::Vec3{1.0f, 1.0f, 1.0f});

            raySurface.coverage = 1.0f;

            if (factors.alphaMode == render::AlphaMode::Mask)
            {
                raySurface.coverage = material.baseColor != assets::k_noTexture ? base.coverage
                    : (factors.baseColorFactor.w >= factors.alphaCutoff ? 1.0f : 0.0f);
            }

            raySurface.blocksShadows = std::max({emissive.x, emissive.y, emissive.z}) < k_emissiveThreshold;

            const math::Vec3 limited{std::clamp(albedo.x, 0.0f, k_maxAlbedo), std::clamp(albedo.y, 0.0f, k_maxAlbedo),
                std::clamp(albedo.z, 0.0f, k_maxAlbedo)};

            return BakeSurface{.albedo = limited, .emissive = emissive, .doubleSided = factors.doubleSided};
        }

        void addTriangles(const SourceScene& scene, const assets::SceneInstance& instance,
            std::vector<RayTriangle>& triangles)
        {
            const SourceMesh& mesh = scene.meshes[instance.mesh];
            const math::Mat4 placement = math::toMatrix(instance.transform);
            const math::Vec3& scale = instance.transform.scale;
            const bool isMirrored = scale.x * scale.y * scale.z < 0.0f;
            const std::vector<render::PbrVertex>& vertices = mesh.geometry.vertices;
            const std::vector<uint32_t>& indices = mesh.geometry.indices;

            for (std::size_t first = 0; first + 2 < indices.size(); first += 3)
            {
                RayTriangle triangle{.corners = {}, .surface = mesh.material};

                for (std::size_t corner = 0; corner < 3; ++corner)
                {
                    const render::PbrVertex& vertex = vertices[indices[first + corner]];
                    triangle.corners[corner] = math::transformPoint(placement,
                        math::Vec3{vertex.position[0], vertex.position[1], vertex.position[2]});
                }

                if (isMirrored)
                {
                    std::swap(triangle.corners[1], triangle.corners[2]);
                }

                triangles.push_back(triangle);
            }
        }

        bool isUnbounced(std::string_view name)
        {
            return std::ranges::any_of(k_unbouncedLightTags, [name](std::string_view tag)
            {
                return name.find(tag) != std::string_view::npos;
            });
        }

        void addLight(const assets::SceneLight& light, std::vector<BakeLight>& lights, BakeSceneReport& report)
        {
            if (light.type == assets::SceneLightType::Directional || isUnbounced(light.name))
            {
                ++report.skippedLights;
                return;
            }

            BakeLight baked{.position = light.transform.position, .radiance = light.color * assets::renderIntensity(light),
                .range = assets::renderRange(light), .direction = {0.0f, 0.0f, -1.0f}, .cosOuter = -1.0f,
                .cosInner = -1.0f, .isSpot = light.type == assets::SceneLightType::Spot};

            if (baked.isSpot)
            {
                const render::SpotLight spot = assets::toSpotLight(light);
                baked.direction = spot.direction;
                baked.cosOuter = std::cos(spot.outerAngle);
                baked.cosInner = std::cos(spot.innerAngle);
            }

            ++report.lights;

            if (light.name.find(k_tubeTag) == std::string::npos)
            {
                lights.push_back(baked);
                return;
            }

            ++report.tubeLights;
            const math::Vec3 axis = light.transform.rotation * math::Vec3{1.0f, 0.0f, 0.0f};
            const float length = std::abs(light.transform.scale.x);
            const math::Vec3 centre = baked.position;
            baked.radiance = baked.radiance * (1.0f / static_cast<float>(k_tubePoints));

            for (uint32_t point = 0; point < k_tubePoints; ++point)
            {
                const float along = (static_cast<float>(point) + 0.5f) / static_cast<float>(k_tubePoints) - 0.5f;
                baked.position = centre + axis * (along * length);
                lights.push_back(baked);
            }
        }

        void tangentFrame(const math::Vec3& normal, math::Vec3& tangent, math::Vec3& bitangent)
        {
            const float sign = std::copysign(1.0f, normal.z);
            const float a = -1.0f / (sign + normal.z);
            const float b = normal.x * normal.y * a;
            tangent = math::Vec3{1.0f + sign * normal.x * normal.x * a, sign * b, -sign * normal.x};
            bitangent = math::Vec3{b, sign + normal.y * normal.y * a, -normal.y};
        }

        int32_t cellIndex(float value, float minimum, float cellSize, int32_t size)
        {
            return std::clamp(static_cast<int32_t>(std::floor((value - minimum) / cellSize)), 0, size - 1);
        }
    } //namespace

    Result<BakeScene> BakeScene::create(const SourceScene& scene, std::span<const TextureMean> means,
        std::optional<math::Vec3> sunTravel)
    {
        std::vector<RaySurface> raySurfaces(scene.materials.size());
        std::vector<BakeSurface> surfaces(scene.materials.size());

        for (std::size_t index = 0; index < scene.materials.size(); ++index)
        {
            surfaces[index] = surfaceOf(scene.materials[index], means, raySurfaces[index]);
        }

        BakeSceneReport report{};
        std::vector<RayTriangle> triangles;

        for (const assets::SceneInstance& instance : scene.instances)
        {
            if (scene.meshes[instance.mesh].alphaMode == render::AlphaMode::Blend || isSkippedNode(scene, instance.node))
            {
                ++report.skippedInstances;
                continue;
            }

            ++report.instances;
            addTriangles(scene, instance, triangles);
        }

        auto tracer = RayTracer::create(triangles, std::move(raySurfaces));

        if (!tracer)
        {
            return tracer.error();
        }

        BakeScene bake(std::move(tracer).value());
        bake.m_surfaces = std::move(surfaces);

        if (sunTravel)
        {
            bake.m_toSun = math::normalize(*sunTravel) * -1.0f;
        }

        for (const assets::SceneLight& light : scene.lights)
        {
            addLight(light, bake.m_lights, report);
        }

        bake.m_report = report;
        bake.buildLightGrid();

        return bake;
    }

    void BakeScene::buildLightGrid()
    {
        m_gridBounds = {};

        for (const BakeLight& light : m_lights)
        {
            const math::Vec3 reach{light.range, light.range, light.range};
            math::expand(m_gridBounds, light.position - reach);
            math::expand(m_gridBounds, light.position + reach);
        }

        if (m_lights.empty())
        {
            m_gridSize = {0, 0, 0};
            return;
        }

        const math::Vec3 extent = m_gridBounds.max - m_gridBounds.min;
        m_cellSize = std::max({k_lightCellSize, extent.x / static_cast<float>(k_maxLightCells),
            extent.y / static_cast<float>(k_maxLightCells), extent.z / static_cast<float>(k_maxLightCells)});

        for (int32_t axis = 0; axis < 3; ++axis)
        {
            m_gridSize[static_cast<std::size_t>(axis)] = std::clamp(
                static_cast<int32_t>(std::ceil(extent[static_cast<std::size_t>(axis)] / m_cellSize)), 1, k_maxLightCells);
        }

        const auto cellCount = static_cast<std::size_t>(m_gridSize.x) * static_cast<std::size_t>(m_gridSize.y)
            * static_cast<std::size_t>(m_gridSize.z);
        std::vector<std::vector<uint32_t>> cells(cellCount);

        for (uint32_t index = 0; index < m_lights.size(); ++index)
        {
            const BakeLight& light = m_lights[index];
            std::array<int32_t, 3> low{};
            std::array<int32_t, 3> high{};

            for (std::size_t axis = 0; axis < 3; ++axis)
            {
                const auto size = m_gridSize[axis];
                low[axis] = cellIndex(light.position[axis] - light.range, m_gridBounds.min[axis], m_cellSize, size);
                high[axis] = cellIndex(light.position[axis] + light.range, m_gridBounds.min[axis], m_cellSize, size);
            }

            for (int32_t z = low[2]; z <= high[2]; ++z)
            {
                for (int32_t y = low[1]; y <= high[1]; ++y)
                {
                    for (int32_t x = low[0]; x <= high[0]; ++x)
                    {
                        cells[static_cast<std::size_t>((z * m_gridSize.y + y) * m_gridSize.x + x)].push_back(index);
                    }
                }
            }
        }

        m_cellStart.assign(cellCount + 1, 0);
        m_cellLights.clear();

        for (std::size_t cell = 0; cell < cellCount; ++cell)
        {
            m_cellStart[cell] = static_cast<uint32_t>(m_cellLights.size());
            m_cellLights.insert(m_cellLights.end(), cells[cell].begin(), cells[cell].end());
        }

        m_cellStart[cellCount] = static_cast<uint32_t>(m_cellLights.size());
    }

    std::span<const uint32_t> BakeScene::lightsNear(const math::Vec3& position) const
    {
        if (m_gridSize.x == 0)
        {
            return {};
        }

        std::array<int32_t, 3> cell{};

        for (std::size_t axis = 0; axis < 3; ++axis)
        {
            if (position[axis] < m_gridBounds.min[axis] || position[axis] > m_gridBounds.max[axis])
            {
                return {};
            }

            cell[axis] = cellIndex(position[axis], m_gridBounds.min[axis], m_cellSize, m_gridSize[axis]);
        }

        const auto index = static_cast<std::size_t>((cell[2] * m_gridSize.y + cell[1]) * m_gridSize.x + cell[0]);

        return std::span<const uint32_t>(m_cellLights).subspan(m_cellStart[index],
            m_cellStart[index + 1] - m_cellStart[index]);
    }

    DirectLight BakeScene::directLight(const math::Vec3& position, const math::Vec3& normal, uint64_t seed) const
    {
        DirectLight direct{};
        const math::Vec3 origin = position + normal * k_surfaceOffset;

        for (const uint32_t index : lightsNear(origin))
        {
            const BakeLight& light = m_lights[index];
            const math::Vec3 toLight = light.position - origin;
            const float distanceSquared = math::lengthSquared(toLight);

            if (distanceSquared >= light.range * light.range)
            {
                continue;
            }

            const float distance = std::sqrt(distanceSquared);
            const math::Vec3 direction = toLight / std::max(distance, 1.0e-6f);
            const float nDotL = math::dot(normal, direction);

            if (nDotL <= 0.0f)
            {
                continue;
            }

            const float ratio = distance / light.range;
            const float window = std::clamp(1.0f - ratio * ratio * ratio * ratio, 0.0f, 1.0f);
            const float falloff = window * window / std::max(distanceSquared, k_minFalloffDistanceSquared);
            const float cone = light.isSpot
                ? smoothstep(light.cosOuter, light.cosInner, math::dot(direction * -1.0f, light.direction)) : 1.0f;
            const float weight = nDotL * falloff * cone;

            if (weight * std::max({light.radiance.x, light.radiance.y, light.radiance.z}) < k_minContribution)
            {
                continue;
            }

            const Ray shadow{.origin = origin, .direction = direction,
                .maxDistance = std::max(distance - k_lightClearance, 0.0f), .seed = seed ^ index};

            if (!m_tracer.isOccluded(shadow))
            {
                direct.local += light.radiance * weight;
            }
        }

        if (m_toSun)
        {
            const float nDotL = math::dot(normal, *m_toSun);

            if (nDotL > 0.0f
                && !m_tracer.isOccluded(Ray{.origin = origin, .direction = *m_toSun, .maxDistance = k_infinity, .seed = seed}))
            {
                direct.sun = nDotL;
            }
        }

        math::Vec3 tangent{};
        math::Vec3 bitangent{};
        tangentFrame(normal, tangent, bitangent);
        uint32_t escaped = 0;

        for (uint32_t ray = 0; ray < k_skyRays; ++ray)
        {
            const uint64_t bits = mixBits(seed + 0x632BE59BD9B4E019ull * (ray + 1));
            const float radius = std::sqrt((static_cast<float>(ray) + unitFloat(bits)) / static_cast<float>(k_skyRays));
            const float angle = 2.0f * math::k_pi * unitFloat(mixBits(bits));
            const float height = std::sqrt(std::max(1.0f - radius * radius, 0.0f));
            const math::Vec3 direction = tangent * (radius * std::cos(angle)) + bitangent * (radius * std::sin(angle))
                + normal * height;

            if (!m_tracer.isOccluded(Ray{.origin = origin, .direction = direction, .maxDistance = k_infinity,
                    .seed = bits}))
            {
                ++escaped;
            }
        }

        direct.sky = static_cast<float>(escaped) / static_cast<float>(k_skyRays);

        return direct;
    }

    const RayTracer& BakeScene::tracer() const noexcept
    {
        return m_tracer;
    }

    const BakeSurface& BakeScene::surface(uint32_t surface) const noexcept
    {
        return m_surfaces[surface];
    }

    const BakeSceneReport& BakeScene::report() const noexcept
    {
        return m_report;
    }
} //namespace lockstep::cooker
