#include "FrameLighting.hpp"

#include <cmath>

namespace lockstep::render
{
    namespace
    {
        constexpr float k_casterNone{-2.0f};
        constexpr float k_casterDirectional{-1.0f};
        constexpr float k_pointKind{0.0f};
        constexpr float k_spotKind{1.0f};
        constexpr float k_spotShadowNear{0.05f};
        constexpr float k_spotShadowFovMargin{1.05f};
        constexpr float k_maxSpotShadowFov{math::radians(170.0f)};
        constexpr float k_directionalDepthScale{2.0f};
        constexpr math::Vec3 k_worldUp{0.0f, 1.0f, 0.0f};
        constexpr math::Vec3 k_fallbackUp{1.0f, 0.0f, 0.0f};
        constexpr float k_parallelThreshold{0.99f};

        void store(float (&target)[4], const math::Vec3& xyz, float w)
        {
            target[0] = xyz.x;
            target[1] = xyz.y;
            target[2] = xyz.z;
            target[3] = w;
        }

        math::Vec3 upFor(const math::Vec3& direction)
        {
            return std::abs(math::dot(direction, k_worldUp)) > k_parallelThreshold ? k_fallbackUp : k_worldUp;
        }

        math::Mat4 spotViewProjection(const SpotLight& light)
        {
            const math::Vec3 direction = math::normalize(light.direction);
            const float fov = std::fmin(light.outerAngle * 2.0f * k_spotShadowFovMargin, k_maxSpotShadowFov);
            const math::Mat4 view = math::lookAt(light.position, light.position + direction, upFor(direction));

            return math::perspective(fov, 1.0f, k_spotShadowNear, light.range) * view;
        }

        float spotNormalBiasPerMetre(const SpotLight& light, float normalBias)
        {
            const float fov = std::fmin(light.outerAngle * 2.0f * k_spotShadowFovMargin, k_maxSpotShadowFov);

            return normalBias * 2.0f * std::tan(fov * 0.5f) / static_cast<float>(k_shadowMapSize);
        }

        math::Mat4 directionalViewProjection(const DirectionalLight& light)
        {
            const math::Vec3 direction = math::normalize(light.direction);
            const float radius = light.shadowRadius;
            const math::Vec3 eye = light.shadowCenter - direction * (radius * k_directionalDepthScale);
            const math::Mat4 view = math::lookAt(eye, light.shadowCenter, upFor(direction));

            return math::ortho(-radius, radius, -radius, radius, 0.0f, radius * k_directionalDepthScale * 2.0f) * view;
        }

        void packPoint(GpuLocalLight& target, const PointLight& light)
        {
            store(target.positionRange, light.position, light.range);
            store(target.color, light.color * light.intensity, 0.0f);
            store(target.directionCosOuter, math::Vec3{0.0f, 0.0f, 0.0f}, -1.0f);
            store(target.cosInnerKind, math::Vec3{1.0f, k_pointKind, 0.0f}, 0.0f);
        }

        void packSpot(GpuLocalLight& target, const SpotLight& light)
        {
            store(target.positionRange, light.position, light.range);
            store(target.color, light.color * light.intensity, 0.0f);
            store(target.directionCosOuter, math::normalize(light.direction), std::cos(light.outerAngle));
            store(target.cosInnerKind, math::Vec3{std::cos(light.innerAngle), k_spotKind, 0.0f}, 0.0f);
        }

        void packShadow(PreparedLighting& prepared, const FrameLights& lights, const RenderSettings& settings,
            uint32_t firstSpotSlot)
        {
            GpuFrameLighting& gpu = prepared.lighting;
            store(gpu.shadowParams, math::Vec3{k_casterNone, settings.shadows.depthBias, 0.0f}, 0.0f);

            if (!settings.shadows.enabled)
            {
                return;
            }

            if (settings.lighting.localLights)
            {
                for (uint32_t index = 0; index < lights.spots.size(); ++index)
                {
                    const SpotLight& spot = lights.spots[index];

                    if (spot.castsShadows)
                    {
                        prepared.caster = ShadowCaster::Spot;
                        prepared.shadowViewProjection = spotViewProjection(spot);
                        store(gpu.shadowParams, math::Vec3{static_cast<float>(firstSpotSlot + index),
                            settings.shadows.depthBias, spotNormalBiasPerMetre(spot, settings.shadows.normalBias)}, 1.0f);
                        gpu.shadowViewProjection = prepared.shadowViewProjection;

                        return;
                    }
                }
            }

            if (settings.lighting.directional && lights.directional && lights.directional->castsShadows)
            {
                const DirectionalLight& directional = *lights.directional;
                const float texelSize = directional.shadowRadius * 2.0f / static_cast<float>(k_shadowMapSize);

                prepared.caster = ShadowCaster::Directional;
                prepared.shadowViewProjection = directionalViewProjection(directional);
                store(gpu.shadowParams, math::Vec3{k_casterDirectional, settings.shadows.depthBias,
                    settings.shadows.normalBias * texelSize}, 0.0f);
                gpu.shadowViewProjection = prepared.shadowViewProjection;
            }
        }
    } //namespace

    bool FrameLights::hasRoom() const
    {
        return points.size() + spots.size() < k_maxLocalLights;
    }

    void FrameLights::clear()
    {
        directional.reset();
        points.clear();
        spots.clear();
        dropped = 0;
    }

    PreparedLighting prepareLighting(const FrameLights& lights, const RenderSettings& settings)
    {
        PreparedLighting prepared{};
        GpuFrameLighting& gpu = prepared.lighting;

        store(gpu.ambient, settings.lighting.ambientColor * settings.lighting.ambientIntensity, 0.0f);
        store(gpu.directionalDirection, math::Vec3{0.0f, -1.0f, 0.0f}, 0.0f);
        store(gpu.directionalColor, math::Vec3{0.0f, 0.0f, 0.0f}, 0.0f);

        if (settings.lighting.directional && lights.directional)
        {
            store(gpu.directionalDirection, math::normalize(lights.directional->direction), 1.0f);
            store(gpu.directionalColor, lights.directional->color * lights.directional->intensity, 0.0f);
        }

        const FogSettings& fog = settings.fog;
        store(gpu.fogColor, fog.color, fog.density);
        store(gpu.fogParams, math::Vec3{fog.start, fog.enabled ? 1.0f : 0.0f, fog.maxAmount}, 0.0f);

        uint32_t count = 0;

        if (settings.lighting.localLights)
        {
            for (const PointLight& point : lights.points)
            {
                packPoint(gpu.lights[count++], point);
            }

            for (const SpotLight& spot : lights.spots)
            {
                packSpot(gpu.lights[count++], spot);
            }
        }

        gpu.counts[0] = count;
        gpu.counts[1] = settings.lighting.specular ? 1u : 0u;
        gpu.counts[2] = 0;
        gpu.counts[3] = 0;

        packShadow(prepared, lights, settings, static_cast<uint32_t>(lights.points.size()));

        return prepared;
    }
} //namespace lockstep::render
