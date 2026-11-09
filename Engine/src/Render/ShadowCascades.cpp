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
    } //namespace

    math::Mat4 atlasTileTransform(uint32_t tile)
    {
        math::Mat4 transform{1.0f};
        transform[0][0] = 0.5f;
        transform[1][1] = 0.5f;
        transform[3][0] = static_cast<float>(tile % 2) - 0.5f;
        transform[3][1] = static_cast<float>(tile / 2) - 0.5f;

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
        const math::Vec3 direction = math::normalize(light.direction);
        const float fov = std::fmin(light.outerAngle * 2.0f * k_spotShadowFovMargin, k_maxSpotShadowFov);
        const math::Mat4 view = math::lookAt(light.position, light.position + direction, upFor(direction));

        ShadowTile result{};
        result.viewProjection = math::perspective(fov, 1.0f, k_spotShadowNear, light.range) * view;
        result.atlasViewProjection = atlasTileTransform(tile) * result.viewProjection;
        result.texelSize = 2.0f * std::tan(fov * 0.5f) / static_cast<float>(k_shadowTileSize);

        return result;
    }
} //namespace lockstep::render
