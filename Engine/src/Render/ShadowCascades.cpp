#include "ShadowCascades.hpp"

#include "FrameAttachments.hpp"

#include <algorithm>
#include <cmath>

namespace lockstep::render
{
    namespace
    {
        constexpr math::Vec3 k_worldUp{0.0f, 1.0f, 0.0f};
        constexpr math::Vec3 k_fallbackUp{1.0f, 0.0f, 0.0f};
        constexpr float k_parallelThreshold{0.99f};
        constexpr float k_casterReach{150.0f};
        constexpr float k_spotShadowNear{0.05f};
        constexpr float k_spotShadowFovMargin{1.05f};
        constexpr float k_maxSpotShadowFov{math::radians(170.0f)};
        constexpr float k_faceEdgeTexels{4.0f};
        constexpr uint32_t k_localTilesPerRow{4};
        constexpr std::array<math::Vec3, k_pointShadowFaces> k_faceDirections{{{1.0f, 0.0f, 0.0f},
            {-1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, -1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f, -1.0f}}};

        math::Vec3 upFor(const math::Vec3& direction)
        {
            return std::abs(math::dot(direction, k_worldUp)) > k_parallelThreshold ? k_fallbackUp : k_worldUp;
        }

        float splitDistance(float nearPlane, float farPlane, float fraction, float lambda)
        {
            const float logarithmic = nearPlane * std::pow(farPlane / nearPlane, fraction);
            const float uniform = nearPlane + (farPlane - nearPlane) * fraction;

            return lambda * logarithmic + (1.0f - lambda) * uniform;
        }

        ShadowTile perspectiveTile(const math::Vec3& position, const math::Vec3& direction, float fov, float range,
            uint32_t tile)
        {
            const math::Mat4 view = math::lookAt(position, position + direction, upFor(direction));

            ShadowTile result{};
            result.viewProjection = math::perspective(fov, 1.0f, k_spotShadowNear, range) * view;
            result.atlasViewProjection = atlasTileTransform(tile) * result.viewProjection;
            result.texelSize = 2.0f * std::tan(fov * 0.5f) / static_cast<float>(shadowTileTexels(tile));

            return result;
        }
    } //namespace

    math::Vec4 shadowTileRect(uint32_t tile)
    {
        const float width = static_cast<float>(k_shadowAtlasWidth);
        const float height = static_cast<float>(k_shadowAtlasHeight);

        if (tile < k_sunShadowTiles)
        {
            const float size = static_cast<float>(k_shadowTileSize);
            return {static_cast<float>(tile % 2) * size / width, static_cast<float>(tile / 2) * size / height,
                size / width, size / height};
        }

        const uint32_t local = tile - k_sunShadowTiles;
        const float size = static_cast<float>(k_localShadowTileSize);
        const float left = static_cast<float>(k_shadowTileSize * 2);

        return {(left + static_cast<float>(local % k_localTilesPerRow) * size) / width,
            static_cast<float>(local / k_localTilesPerRow) * size / height, size / width, size / height};
    }

    uint32_t shadowTileTexels(uint32_t tile)
    {
        return tile < k_sunShadowTiles ? k_shadowTileSize : k_localShadowTileSize;
    }

    math::Mat4 atlasTileTransform(uint32_t tile)
    {
        const math::Vec4 rect = shadowTileRect(tile);
        math::Mat4 transform{1.0f};
        transform[0][0] = rect.z;
        transform[1][1] = rect.w;
        transform[3][0] = 2.0f * rect.x + rect.z - 1.0f;
        transform[3][1] = 2.0f * rect.y + rect.w - 1.0f;

        return transform;
    }

    void fitSunCascades(const Camera& camera, float aspectRatio, const math::Vec3& direction,
        const ShadowSettings& settings, uint32_t count, std::array<ShadowTile, k_maxShadowTiles>& tiles)
    {
        const math::Vec3 forward = math::normalize(camera.target - camera.position);
        const float tanY = std::tan(camera.verticalFov * 0.5f);
        const float tanX = tanY * aspectRatio;
        const float slope2 = tanX * tanX + tanY * tanY;
        const float nearPlane = std::max(camera.nearPlane, 0.01f);
        const float farPlane = std::max(settings.distance, nearPlane * 2.0f);
        const float lambda = std::clamp(settings.splitLambda, 0.0f, 1.0f);
        const math::Mat4 lightRotation = math::lookAt(math::Vec3{0.0f, 0.0f, 0.0f}, direction, upFor(direction));

        for (uint32_t cascade = 0; cascade < count; ++cascade)
        {
            const float sliceNear = splitDistance(nearPlane, farPlane, static_cast<float>(cascade) / static_cast<float>(count), lambda);
            const float sliceFar = splitDistance(nearPlane, farPlane, static_cast<float>(cascade + 1) / static_cast<float>(count), lambda);

            const float centreDistance = std::min((sliceFar + sliceNear) * (1.0f + slope2) * 0.5f, sliceFar);
            const float farOffset = sliceFar - centreDistance;
            const float radius = std::ceil(std::sqrt(farOffset * farOffset + sliceFar * sliceFar * slope2) * 16.0f) / 16.0f;
            const math::Vec3 centre = camera.position + forward * centreDistance;

            const float texelSize = radius * 2.0f / static_cast<float>(k_shadowTileSize);
            math::Vec3 lightCentre = math::transformPoint(lightRotation, centre);
            lightCentre.x = std::floor(lightCentre.x / texelSize) * texelSize;
            lightCentre.y = std::floor(lightCentre.y / texelSize) * texelSize;

            const math::Mat4 projection = math::ortho(lightCentre.x - radius, lightCentre.x + radius,
                lightCentre.y - radius, lightCentre.y + radius, -lightCentre.z - radius - k_casterReach,
                -lightCentre.z + radius);

            ShadowTile& tile = tiles[cascade];
            tile.viewProjection = projection * lightRotation;
            tile.atlasViewProjection = atlasTileTransform(cascade) * tile.viewProjection;
            tile.texelSize = texelSize;
        }
    }

    ShadowTile fitSpotShadow(const SpotLight& light, uint32_t tile)
    {
        const float fov = std::fmin(light.outerAngle * 2.0f * k_spotShadowFovMargin, k_maxSpotShadowFov);

        return perspectiveTile(light.position, math::normalize(light.direction), fov, light.range, tile);
    }

    ShadowTile fitPointShadowFace(const PointLight& light, uint32_t face, uint32_t tile)
    {
        const float edge = 2.0f * k_faceEdgeTexels / static_cast<float>(shadowTileTexels(tile));
        const float fov = 2.0f * std::atan(1.0f / (1.0f - edge));

        return perspectiveTile(light.position, k_faceDirections[face], fov, light.range, tile);
    }
} //namespace lockstep::render
