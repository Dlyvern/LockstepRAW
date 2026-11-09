#include "FrameCulling.hpp"

#include "RendererState.hpp"

#include <algorithm>
#include <cmath>

namespace lockstep::render
{
    namespace
    {
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
        blended.clear();

        for (std::vector<uint32_t>& casters : shadowCasters)
        {
            casters.clear();
        }
    }

    void buildDrawLists(RendererState& state, const math::Mat4& viewProjection)
    {
        FrameDrawLists& lists = state.drawLists;
        lists.clear();

        const bool cull = state.settings.culling.frustumCulling;
        const Frustum cameraFrustum = frustumFromMatrix(viewProjection);
        const PreparedLighting& prepared = state.prepared;

        std::array<Frustum, k_maxShadowTiles> tileFrusta{};
        std::array<bool, k_maxShadowTiles> tileActive{};

        for (uint32_t tile = 0; tile < k_maxShadowTiles; ++tile)
        {
            tileActive[tile] = tile < prepared.cascadeCount || (prepared.spotTile && *prepared.spotTile == tile);
            tileFrusta[tile] = frustumFromMatrix(prepared.tiles[tile].viewProjection);
        }

        for (uint32_t index = 0; index < state.draws.size(); ++index)
        {
            DrawItem& item = state.draws[index];
            item.cameraDistance = math::length((item.worldBounds.min + item.worldBounds.max) * 0.5f - state.camera.position);

            if (!cull || intersects(cameraFrustum, item.worldBounds))
            {
                (item.material->alphaMode == AlphaMode::Blend ? lists.blended : lists.opaque).push_back(index);
            }

            if (!castsShadow(item))
            {
                continue;
            }

            for (uint32_t tile = 0; tile < k_maxShadowTiles; ++tile)
            {
                if (tileActive[tile] && (!cull || intersects(tileFrusta[tile], item.worldBounds)))
                {
                    lists.shadowCasters[tile].push_back(index);
                }
            }
        }

        const std::vector<DrawItem>& draws = state.draws;

        std::sort(lists.opaque.begin(), lists.opaque.end(), [&draws](uint32_t left, uint32_t right)
        {
            return draws[left].cameraDistance < draws[right].cameraDistance;
        });

        std::sort(lists.blended.begin(), lists.blended.end(), [&draws](uint32_t left, uint32_t right)
        {
            return draws[left].cameraDistance > draws[right].cameraDistance;
        });
    }
} //namespace lockstep::render
