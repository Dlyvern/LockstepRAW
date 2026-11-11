#include "FrameCulling.hpp"

#include "RendererState.hpp"
#include "VelaConversions.hpp"
#include "ViewModel.hpp"

#include <algorithm>
#include <bit>
#include <cstdint>
#include <cmath>
#include <span>
#include <tuple>
#include <utility>
#include <vector>

namespace lockstep::render
{
    namespace
    {
        constexpr float k_minLocalCasterAngle{0.004f};
        constexpr float k_receiverFovMargin{1.1f};
        constexpr float k_lightMaskMargin{0.25f};
        constexpr float k_maxReceiverFov{math::radians(170.0f)};
        constexpr uint64_t k_keyMix{0x9E3779B97F4A7C15ull};
        constexpr uint64_t k_keyPrime{0x100000001B3ull};

        struct CasterPlanes
        {
            std::array<math::Vec4, 6> planes{};
            uint32_t count{0};
        };

        math::Vec4 row(const math::Mat4& matrix, int index)
        {
            return {matrix[0][index], matrix[1][index], matrix[2][index], matrix[3][index]};
        }

        math::Vec4 normalizedPlane(const math::Vec4& plane)
        {
            const float length = std::sqrt(plane.x * plane.x + plane.y * plane.y + plane.z * plane.z);

            return length > 0.0f ? math::Vec4{plane.x / length, plane.y / length, plane.z / length, plane.w / length}
                : plane;
        }

        uint64_t mixWord(uint64_t key, uint64_t word)
        {
            key ^= word + k_keyMix + (key << 6) + (key >> 2);

            return key * k_keyPrime;
        }

        uint64_t mixKey(uint64_t key, const math::Mat4& matrix)
        {
            for (int column = 0; column < 4; ++column)
            {
                for (int component = 0; component < 4; ++component)
                {
                    key = mixWord(key, std::bit_cast<uint32_t>(matrix[column][component]));
                }
            }

            return key;
        }

        uint64_t shadowCasterKey(const DrawItem& item)
        {
            uint64_t key = mixWord(k_keyPrime, reinterpret_cast<std::uintptr_t>(item.mesh));
            key = mixWord(key, reinterpret_cast<std::uintptr_t>(item.material));

            return mixKey(key, item.model);
        }

        bool castsShadow(const DrawItem& item)
        {
            return item.material->shading == Shading::Lit && item.material->alphaMode != AlphaMode::Blend;
        }

        bool castsIntoLocalTile(const math::Aabb& bounds, const math::Vec4& lightSphere)
        {
            const math::Vec3 light{lightSphere.x, lightSphere.y, lightSphere.z};
            const math::Vec3 nearest = math::Vec3{std::clamp(light.x, bounds.min.x, bounds.max.x),
                std::clamp(light.y, bounds.min.y, bounds.max.y), std::clamp(light.z, bounds.min.z, bounds.max.z)};
            const float distance = math::length(nearest - light);

            if (distance > lightSphere.w)
            {
                return false;
            }

            const float radius = math::length(bounds.max - bounds.min) * 0.5f;

            return radius >= k_minLocalCasterAngle * math::length((bounds.min + bounds.max) * 0.5f - light);
        }

        bool coversEnoughTexels(const DrawItem& item, uint32_t tile, const PreparedLighting& prepared,
            float minTexels)
        {
            if (minTexels <= 0.0f || item.material->kind == MeshKind::Skinned)
            {
                return true;
            }

            const float diameter = math::length(item.worldBounds.max - item.worldBounds.min);
            float texelSize = prepared.tiles[tile].texelSize;

            if (tile >= k_sunShadowTiles)
            {
                const math::Vec4& sphere = prepared.casterSpheres[tile - k_sunShadowTiles];
                const math::Vec3 centre = (item.worldBounds.min + item.worldBounds.max) * 0.5f;
                texelSize *= std::max(math::length(centre - math::Vec3{sphere.x, sphere.y, sphere.z}), k_localShadowNear);
            }

            return diameter >= texelSize * minTexels;
        }

        bool castsIntoTile(const DrawItem& item, uint32_t tile, const PreparedLighting& prepared,
            const ShadowSettings& settings, float minTexels)
        {
            if (!coversEnoughTexels(item, tile, prepared, minTexels))
            {
                return false;
            }

            if (tile >= k_sunShadowTiles)
            {
                return castsIntoLocalTile(item.worldBounds, prepared.casterSpheres[tile - k_sunShadowTiles]);
            }

            if (item.material->kind == MeshKind::Skinned && tile >= settings.skinnedCasterCascades)
            {
                return false;
            }

            return item.material->alphaMode != AlphaMode::Mask || tile < settings.maskedCasterCascades;
        }

        bool intersectsPlanes(std::span<const math::Vec4> planes, const math::Aabb& box)
        {
            const math::Vec3 centre = (box.min + box.max) * 0.5f;
            const math::Vec3 extent = (box.max - box.min) * 0.5f;

            for (const math::Vec4& plane : planes)
            {
                const float distance = plane.x * centre.x + plane.y * centre.y + plane.z * centre.z + plane.w;
                const float reach = std::abs(plane.x) * extent.x + std::abs(plane.y) * extent.y + std::abs(plane.z) * extent.z;

                if (distance + reach < 0.0f)
                {
                    return false;
                }
            }

            return true;
        }

        CasterPlanes sunCasterPlanes(const RendererState& state)
        {
            Camera receivers = state.camera;
            receivers.verticalFov = std::min(std::max(receivers.verticalFov, receivers.viewModelFov) * k_receiverFovMargin,
                k_maxReceiverFov);
            const Frustum view = frustumFromMatrix(toProjectionMatrix(receivers, state.aspectRatio) * state.view);
            const math::Vec3& travel = state.prepared.sunTravel;

            CasterPlanes result{};

            for (const math::Vec4& plane : view.planes)
            {
                if (plane.x * travel.x + plane.y * travel.y + plane.z * travel.z <= 0.0f)
                {
                    result.planes[result.count++] = plane;
                }
            }

            return result;
        }

        uint32_t lightMaskFor(const math::Aabb& bounds, std::span<const math::Vec4> lights)
        {
            uint32_t mask = 0;

            for (uint32_t index = 0; index < lights.size(); ++index)
            {
                const math::Vec4& light = lights[index];
                const math::Vec3 centre{light.x, light.y, light.z};
                const math::Vec3 nearest{std::clamp(centre.x, bounds.min.x, bounds.max.x),
                    std::clamp(centre.y, bounds.min.y, bounds.max.y), std::clamp(centre.z, bounds.min.z, bounds.max.z)};

                if (math::length(nearest - centre) < light.w + k_lightMaskMargin)
                {
                    mask |= 1u << index;
                }
            }

            return mask;
        }

        math::Vec4 slabPlane(const math::Mat4& viewProjection, int axis, float boundary, float side)
        {
            return normalizedPlane((row(viewProjection, axis) - row(viewProjection, 3) * boundary) * side);
        }

        bool reachesPlane(const math::Vec4& plane, const math::Vec4& sphere)
        {
            return plane.x * sphere.x + plane.y * sphere.y + plane.z * sphere.z + plane.w >= -sphere.w;
        }

        std::pair<uint32_t, uint32_t> coveredCells(const math::Mat4& viewProjection, int axis, uint32_t cells,
            const math::Vec4& sphere)
        {
            uint32_t first = cells;
            uint32_t last = 0;

            for (uint32_t cell = 0; cell < cells; ++cell)
            {
                const float low = static_cast<float>(cell) / static_cast<float>(cells) * 2.0f - 1.0f;
                const float high = static_cast<float>(cell + 1) / static_cast<float>(cells) * 2.0f - 1.0f;

                if (reachesPlane(slabPlane(viewProjection, axis, low, 1.0f), sphere)
                    && reachesPlane(slabPlane(viewProjection, axis, high, -1.0f), sphere))
                {
                    first = std::min(first, cell);
                    last = cell;
                }
            }

            return {first, last};
        }

        void writeLightCells(GpuFrameLighting& lighting, std::span<const math::Vec4> lights,
            const math::Mat4& viewProjection, uint32_t width, uint32_t height)
        {
            std::fill(std::begin(lighting.lightCells), std::end(lighting.lightCells), 0u);
            lighting.lightCellScale[0] = static_cast<float>(k_lightCellsX) / static_cast<float>(std::max(width, 1u));
            lighting.lightCellScale[1] = static_cast<float>(k_lightCellsY) / static_cast<float>(std::max(height, 1u));
            const math::Vec4 nearPlane = normalizedPlane(row(viewProjection, 2));

            for (uint32_t index = 0; index < lights.size(); ++index)
            {
                const math::Vec4& sphere = lights[index];

                if (!reachesPlane(nearPlane, sphere))
                {
                    continue;
                }

                const auto [firstColumn, lastColumn] = coveredCells(viewProjection, 0, k_lightCellsX, sphere);
                const auto [firstRow, lastRow] = coveredCells(viewProjection, 1, k_lightCellsY, sphere);
                const uint32_t bit = 1u << index;

                for (uint32_t y = firstRow; y <= lastRow && y < k_lightCellsY; ++y)
                {
                    for (uint32_t x = firstColumn; x <= lastColumn && x < k_lightCellsX; ++x)
                    {
                        lighting.lightCells[y * k_lightCellsX + x] |= bit;
                    }
                }
            }
        }

        void sortByShadowState(std::vector<uint32_t>& list, const std::vector<DrawItem>& draws)
        {
            auto key = [&draws](uint32_t index)
            {
                const DrawItem& item = draws[index];
                const MaterialRecord* masked = item.material->maskedShadow ? item.material : nullptr;

                return std::tuple(masked, item.material->kind, item.mesh);
            };

            std::sort(list.begin(), list.end(), [&key](uint32_t left, uint32_t right)
            {
                return key(left) < key(right);
            });
        }

        void sortByDistance(std::vector<uint32_t>& list, const std::vector<DrawItem>& draws, bool isFrontToBack)
        {
            std::sort(list.begin(), list.end(), [&draws, isFrontToBack](uint32_t left, uint32_t right)
            {
                return isFrontToBack ? draws[left].cameraDistance < draws[right].cameraDistance
                    : draws[left].cameraDistance > draws[right].cameraDistance;
            });
        }
    } //namespace

    Frustum frustumFromMatrix(const math::Mat4& viewProjection)
    {
        const math::Vec4 x = row(viewProjection, 0);
        const math::Vec4 y = row(viewProjection, 1);
        const math::Vec4 z = row(viewProjection, 2);
        const math::Vec4 w = row(viewProjection, 3);

        Frustum frustum{};
        frustum.planes[0] = normalizedPlane(w + x);
        frustum.planes[1] = normalizedPlane(w - x);
        frustum.planes[2] = normalizedPlane(w + y);
        frustum.planes[3] = normalizedPlane(w - y);
        frustum.planes[4] = normalizedPlane(z);
        frustum.planes[5] = normalizedPlane(w - z);

        return frustum;
    }

    bool intersects(const Frustum& frustum, const math::Aabb& box)
    {
        return intersectsPlanes(frustum.planes, box);
    }

    void FrameDrawLists::clear()
    {
        opaque.clear();
        decals.clear();
        blended.clear();
        viewModelOpaque.clear();
        viewModelBlended.clear();

        for (std::vector<uint32_t>& casters : shadowCasters)
        {
            casters.clear();
        }

        shadowKeys.fill(0);
        shadowAnimated.fill(false);
    }

    void buildDrawLists(RendererState& state, const math::Mat4& viewProjection,
        const math::Mat4& viewModelViewProjection)
    {
        FrameDrawLists& lists = state.drawLists;
        lists.clear();

        const bool cull = state.settings.culling.frustumCulling;
        const Frustum cameraFrustum = frustumFromMatrix(viewProjection);
        const Frustum viewModelFrustum = frustumFromMatrix(viewModelViewProjection);
        const PreparedLighting& prepared = state.prepared;

        std::array<Frustum, k_maxShadowTiles> tileFrusta{};
        std::array<bool, k_maxShadowTiles> tileActive{};

        for (uint32_t tile = 0; tile < k_maxShadowTiles; ++tile)
        {
            tileActive[tile] = prepared.isTileActive(tile);
            tileFrusta[tile] = frustumFromMatrix(prepared.tiles[tile].viewProjection);
        }

        const bool limitSunCasters = cull && state.settings.culling.sunCasterCulling && prepared.cascadeCount > 0
            && !state.reflection.active;
        const CasterPlanes sunCasters = limitSunCasters ? sunCasterPlanes(state) : CasterPlanes{};

        const GpuFrameLighting& lighting = prepared.lighting;
        std::array<math::Vec4, k_maxLocalLights> lightSpheres{};
        const uint32_t lightCount = std::min(lighting.counts[0], k_maxLocalLights);

        for (uint32_t light = 0; light < lightCount; ++light)
        {
            const float* sphere = lighting.lights[light].positionRange;
            const float* tube = lighting.lights[light].tube;
            const float halfLength = std::sqrt(tube[0] * tube[0] + tube[1] * tube[1] + tube[2] * tube[2]);
            lightSpheres[light] = math::Vec4{sphere[0], sphere[1], sphere[2], sphere[3] + halfLength};
        }

        const std::span<const math::Vec4> lights(lightSpheres.data(), lightCount);
        writeLightCells(state.prepared.lighting, lights, viewProjection, state.frameWidth, state.frameHeight);

        for (uint32_t index = 0; index < state.draws.size(); ++index)
        {
            DrawItem& item = state.draws[index];
            item.cameraDistance = math::length((item.worldBounds.min + item.worldBounds.max) * 0.5f - state.camera.position);

            item.lightMask = isViewModelDraw(item.layers) ? 0xFFFFFFFFu : lightMaskFor(item.worldBounds, lights);

            if (isViewModelDraw(item.layers))
            {
                if (!cull || intersects(viewModelFrustum, item.worldBounds))
                {
                    (item.material->alphaMode == AlphaMode::Blend ? lists.viewModelBlended : lists.viewModelOpaque)
                        .push_back(index);
                }

                continue;
            }

            const bool isSeen = (item.layers & state.camera.layers) != 0;

            if (isSeen && (!cull || intersects(cameraFrustum, item.worldBounds)))
            {
                const bool isBlended = item.material->alphaMode == AlphaMode::Blend;
                (isBlended ? (item.isDecal ? lists.decals : lists.blended) : lists.opaque).push_back(index);
            }

            if ((item.layers & state.camera.shadowLayers) == 0 || !castsShadow(item))
            {
                continue;
            }

            const bool reachesView = intersectsPlanes(std::span(sunCasters.planes.data(), sunCasters.count),
                item.worldBounds);
            const uint64_t casterKey = shadowCasterKey(item);

            for (uint32_t tile = 0; tile < k_maxShadowTiles; ++tile)
            {
                if (!tileActive[tile] || !castsIntoTile(item, tile, prepared, state.settings.shadows, state.settings.shadowDetail.minCasterTexels)
                    || (cull && !intersects(tileFrusta[tile], item.worldBounds)))
                {
                    continue;
                }

                lists.shadowKeys[tile] += casterKey;
                lists.shadowAnimated[tile] = lists.shadowAnimated[tile] || item.material->kind == MeshKind::Skinned;

                if (tile >= k_sunShadowTiles || reachesView)
                {
                    lists.shadowCasters[tile].push_back(index);
                }
            }
        }

        for (uint32_t tile = 0; tile < k_maxShadowTiles; ++tile)
        {
            lists.shadowKeys[tile] = mixKey(lists.shadowKeys[tile], prepared.tiles[tile].atlasViewProjection);
        }

        for (std::vector<uint32_t>& casters : lists.shadowCasters)
        {
            sortByShadowState(casters, state.draws);
        }

        sortByDistance(lists.opaque, state.draws, true);
        sortByDistance(lists.blended, state.draws, false);
        sortByDistance(lists.viewModelOpaque, state.draws, true);
        sortByDistance(lists.viewModelBlended, state.draws, false);
    }
} //namespace lockstep::render
