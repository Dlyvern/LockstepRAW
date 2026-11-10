#include "FrameCulling.hpp"

#include "RendererState.hpp"
#include "ViewModel.hpp"

#include <algorithm>
#include <cmath>
#include <vector>

namespace lockstep::render
{
    namespace
    {
        constexpr float k_minLocalCasterAngle{0.004f};

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

        bool castsIntoTile(const DrawItem& item, uint32_t tile, const PreparedLighting& prepared,
            const ShadowSettings& settings)
        {
            if (tile >= k_sunShadowTiles)
            {
                return castsIntoLocalTile(item.worldBounds, prepared.casterSpheres[tile - k_sunShadowTiles]);
            }

            return item.material->alphaMode != AlphaMode::Mask || tile < settings.maskedCasterCascades;
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
        const math::Vec3 centre = (box.min + box.max) * 0.5f;
        const math::Vec3 extent = (box.max - box.min) * 0.5f;

        for (const math::Vec4& plane : frustum.planes)
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

        for (uint32_t index = 0; index < state.draws.size(); ++index)
        {
            DrawItem& item = state.draws[index];
            item.cameraDistance = math::length((item.worldBounds.min + item.worldBounds.max) * 0.5f - state.camera.position);

            if (isViewModelDraw(item.layers))
            {
                if (!cull || intersects(viewModelFrustum, item.worldBounds))
                {
                    (item.material->alphaMode == AlphaMode::Blend ? lists.viewModelBlended : lists.viewModelOpaque)
                        .push_back(index);
                }

                continue;
            }

            if ((item.layers & state.camera.layers) == 0)
            {
                continue;
            }

            if (!cull || intersects(cameraFrustum, item.worldBounds))
            {
                const bool isBlended = item.material->alphaMode == AlphaMode::Blend;
                (isBlended ? (item.isDecal ? lists.decals : lists.blended) : lists.opaque).push_back(index);
            }

            if (!castsShadow(item))
            {
                continue;
            }

            for (uint32_t tile = 0; tile < k_maxShadowTiles; ++tile)
            {
                if (tileActive[tile] && castsIntoTile(item, tile, prepared, state.settings.shadows)
                    && (!cull || intersects(tileFrusta[tile], item.worldBounds)))
                {
                    lists.shadowCasters[tile].push_back(index);
                }
            }
        }

        sortByDistance(lists.opaque, state.draws, true);
        sortByDistance(lists.blended, state.draws, false);
        sortByDistance(lists.viewModelOpaque, state.draws, true);
        sortByDistance(lists.viewModelBlended, state.draws, false);
    }
} //namespace lockstep::render
