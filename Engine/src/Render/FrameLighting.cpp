#include "FrameLighting.hpp"

#include "SkyModel.hpp"

#include <algorithm>
#include <cmath>

namespace lockstep::render
{
    namespace
    {
        constexpr float k_noSpotShadow{-1.0f};
        constexpr float k_pointKind{0.0f};
        constexpr float k_spotKind{1.0f};
        constexpr float k_sunDiskCosine{0.99994f};
        constexpr float k_sunDiskBrightness{40.0f};

        void store(float (&target)[4], const math::Vec3& xyz, float w)
        {
            target[0] = xyz.x;
            target[1] = xyz.y;
            target[2] = xyz.z;
            target[3] = w;
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

        std::optional<DirectionalLight> frameSun(const FrameLights& lights, const RenderSettings& settings)
        {
            if (!settings.lighting.directional)
            {
                return std::nullopt;
            }

            if (lights.directional)
            {
                return lights.directional;
            }

            if (!settings.sky.enabled)
            {
                return std::nullopt;
            }

            return DirectionalLight{
                .direction = sunTravelDirection(settings.sky.sunAzimuth, settings.sky.sunElevation),
                .color = skySunRadiance(settings.sky),
                .intensity = 1.0f,
                .castsShadows = true};
        }

        void packSky(GpuFrameLighting& gpu, const RenderSettings& settings, const math::Vec3& sunDirection,
            const math::Vec3& sunRadiance)
        {
            const SkyCoefficients sky = computeSky(settings.sky, sunDirection * -1.0f, sunRadiance);

            store(gpu.skyA, sky.perezA, 0.0f);
            store(gpu.skyB, sky.perezB, 0.0f);
            store(gpu.skyC, sky.perezC, 0.0f);
            store(gpu.skyD, sky.perezD, 0.0f);
            store(gpu.skyE, sky.perezE, 0.0f);
            store(gpu.skyZenith, sky.zenith, sky.radianceScale);
            store(gpu.skyGround, sky.ground, settings.sky.enabled ? 1.0f : 0.0f);

            const bool showDisk = settings.sky.enabled && settings.sky.sunDisk;
            store(gpu.sunDisk, sunRadiance * k_sunDiskBrightness, showDisk ? k_sunDiskCosine : 0.0f);

            const ImageBasedLightingSettings& ibl = settings.ibl;
            store(gpu.iblParams, math::Vec3{ibl.diffuseIntensity, ibl.specularIntensity, 0.0f},
                ibl.enabled && settings.sky.enabled ? 1.0f : 0.0f);
        }

        void packShadows(PreparedLighting& prepared, const FrameLights& lights, const RenderSettings& settings,
            const std::optional<DirectionalLight>& sun, const Camera& camera, float aspectRatio)
        {
            GpuFrameLighting& gpu = prepared.lighting;
            const ShadowSettings& shadows = settings.shadows;
            store(gpu.shadowParams, math::Vec3{0.0f, 0.0f, shadows.normalBias},
                shadows.debugCascades ? 1.0f : 0.0f);
            store(gpu.spotShadowParams, math::Vec3{k_noSpotShadow, 0.0f, 0.0f}, 0.0f);
            store(gpu.cascadeTexelSize, math::Vec3{0.0f, 0.0f, 0.0f}, 0.0f);

            if (!shadows.enabled)
            {
                return;
            }

            uint32_t sunTiles = k_maxShadowTiles;

            if (settings.lighting.localLights)
            {
                const auto caster = std::find_if(lights.spots.begin(), lights.spots.end(),
                    [](const SpotLight& spot) { return spot.castsShadows; });

                if (caster != lights.spots.end())
                {
                    const uint32_t tile = k_maxShadowTiles - 1;
                    const ShadowTile spotTile = fitSpotShadow(*caster, tile);
                    const float slot = static_cast<float>(lights.points.size()
                        + static_cast<std::size_t>(caster - lights.spots.begin()));

                    prepared.spotTile = tile;
                    prepared.tiles[tile] = spotTile;
                    sunTiles = tile;
                    store(gpu.spotShadowParams, math::Vec3{slot, shadows.normalBias * spotTile.texelSize, 0.0f},
                        static_cast<float>(tile));
                }
            }

            if (sun && sun->castsShadows)
            {
                const uint32_t count = std::clamp(shadows.cascadeCount, 1u, sunTiles);
                fitSunCascades(camera, aspectRatio, math::normalize(sun->direction), shadows, count, prepared.tiles);
                prepared.cascadeCount = count;
                gpu.shadowParams[0] = static_cast<float>(count);

                for (uint32_t cascade = 0; cascade < count; ++cascade)
                {
                    gpu.cascadeTexelSize[cascade] = prepared.tiles[cascade].texelSize;
                }
            }

            for (uint32_t tile = 0; tile < k_maxShadowTiles; ++tile)
            {
                gpu.shadowTileViewProjection[tile] = prepared.tiles[tile].atlasViewProjection;
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

    uint32_t PreparedLighting::tileCount() const
    {
        return cascadeCount + (spotTile ? 1u : 0u);
    }

    PreparedLighting prepareLighting(const FrameLights& lights, const RenderSettings& settings, const Camera& camera,
        float aspectRatio)
    {
        PreparedLighting prepared{};
        GpuFrameLighting& gpu = prepared.lighting;

        store(gpu.ambient, settings.lighting.ambientColor * settings.lighting.ambientIntensity, 0.0f);

        const std::optional<DirectionalLight> sun = frameSun(lights, settings);
        const math::Vec3 skySunDirection = sun ? math::normalize(sun->direction)
            : sunTravelDirection(settings.sky.sunAzimuth, settings.sky.sunElevation);
        const math::Vec3 sunRadiance = sun ? sun->color * sun->intensity : math::Vec3{0.0f, 0.0f, 0.0f};

        prepared.hasSun = sun.has_value();
        store(gpu.sunDirection, skySunDirection, sun ? 1.0f : 0.0f);
        store(gpu.sunRadiance, sunRadiance, 0.0f);
        packSky(gpu, settings, skySunDirection, sunRadiance);

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

        packShadows(prepared, lights, settings, sun, camera, aspectRatio);

        return prepared;
    }
} //namespace lockstep::render
