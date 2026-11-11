#include "FrameLighting.hpp"

#include "Atmosphere.hpp"
#include "FrameCulling.hpp"
#include "SkyModel.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <utility>

namespace lockstep::render
{
    namespace
    {
        constexpr float k_noShadow{-1.0f};
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

        math::Vec3 unitOr(const math::Vec3& direction, const math::Vec3& fallback)
        {
            const float length = math::length(direction);

            return length > 1e-5f ? direction * (1.0f / length) : fallback;
        }

        void packProfile(GpuLocalLight& target, LightProfile profile, const math::Vec3& side, bool enabled)
        {
            const float index = enabled ? static_cast<float>(profile) : 0.0f;
            store(target.profile, unitOr(side, math::Vec3{1.0f, 0.0f, 0.0f}), index);
        }

        void packPoint(GpuLocalLight& target, const PointLight& light, const LightShapeSettings& shapes)
        {
            store(target.positionRange, light.position, light.range);
            store(target.color, light.color * light.intensity, std::max(light.volumetric, 0.0f));
            store(target.directionCosOuter, unitOr(light.profileAxis, math::Vec3{0.0f, -1.0f, 0.0f}), -1.0f);
            store(target.cosInnerKind, math::Vec3{1.0f, k_pointKind, k_noShadow}, 0.0f);

            const float halfLength = shapes.tubeLights ? std::max(light.tubeLength, 0.0f) * 0.5f : 0.0f;
            store(target.tube, unitOr(light.tubeAxis, math::Vec3{1.0f, 0.0f, 0.0f}) * halfLength, halfLength);
            packProfile(target, light.profile, light.profileSide, shapes.profiles);
        }

        void packSpot(GpuLocalLight& target, const SpotLight& light, const LightShapeSettings& shapes)
        {
            store(target.positionRange, light.position, light.range);
            store(target.color, light.color * light.intensity, std::max(light.volumetric, 0.0f));
            store(target.directionCosOuter, math::normalize(light.direction), std::cos(light.outerAngle));
            store(target.cosInnerKind, math::Vec3{std::cos(light.innerAngle), k_spotKind, k_noShadow}, 0.0f);
            store(target.tube, math::Vec3{0.0f, 0.0f, 0.0f}, 0.0f);
            packProfile(target, light.profile, light.profileSide, shapes.profiles);
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
            store(gpu.skyGlow, sky.glow, std::max(settings.sky.glowHeight, 0.001f));
            store(gpu.skyGlowShape, math::Vec3{std::max(settings.sky.glowSpread, 0.0f),
                std::clamp(settings.sky.glowOpposite, 0.0f, 1.0f), 0.0f}, 0.0f);
            store(gpu.skyZenithTint, settings.sky.zenithTint, 0.0f);

            const bool showDisk = settings.sky.enabled && settings.sky.sunDisk;
            store(gpu.sunDisk, sunRadiance * k_sunDiskBrightness, showDisk ? k_sunDiskCosine : 0.0f);

            const ImageBasedLightingSettings& ibl = settings.ibl;
            store(gpu.iblParams, math::Vec3{ibl.diffuseIntensity, ibl.specularIntensity, 0.0f},
                ibl.enabled && settings.sky.enabled ? 1.0f : 0.0f);
        }

        struct ShadowCandidate
        {
            uint32_t slot{0};
            float distance{0.0f};
            const PointLight* point{nullptr};
            const SpotLight* spot{nullptr};
        };

        bool reachesView(const Frustum& view, const math::Vec3& position, float range)
        {
            const math::Vec3 reach{range, range, range};

            return intersects(view, math::Aabb{.min = position - reach, .max = position + reach});
        }

        uint32_t gatherShadowCandidates(const FrameLights& lights, const LightingView& view,
            std::array<ShadowCandidate, k_maxLocalLights>& candidates)
        {
            const Frustum frustum = frustumFromMatrix(view.viewProjection);
            uint32_t count = 0;
            uint32_t slot = 0;

            for (const PointLight& point : lights.points)
            {
                if (point.castsShadows && reachesView(frustum, point.position, point.range))
                {
                    const float distance = math::length(point.position - view.camera.position);
                    candidates[count++] = {.slot = slot, .distance = distance, .point = &point};
                }

                ++slot;
            }

            for (const SpotLight& spot : lights.spots)
            {
                if (spot.castsShadows && reachesView(frustum, spot.position, spot.range))
                {
                    const float distance = math::length(spot.position - view.camera.position);
                    candidates[count++] = {.slot = slot, .distance = distance, .spot = &spot};
                }

                ++slot;
            }

            std::sort(candidates.begin(), candidates.begin() + count,
                [](const ShadowCandidate& left, const ShadowCandidate& right) { return left.distance < right.distance; });

            return count;
        }

        void packLocalShadows(PreparedLighting& prepared, const FrameLights& lights, const ShadowSettings& shadows,
            const LightingView& view)
        {
            std::array<ShadowCandidate, k_maxLocalLights> candidates{};
            const uint32_t count = gatherShadowCandidates(lights, view, candidates);
            const uint32_t budget = std::min(shadows.maxLocalLights, k_maxShadowedLocalLights);
            uint32_t tile = k_sunShadowTiles;

            for (uint32_t index = 0; index < count && prepared.shadowedLocalLights < budget; ++index)
            {
                const ShadowCandidate& candidate = candidates[index];
                const uint32_t needed = candidate.point != nullptr ? k_pointShadowFaces : 1;

                if (tile + needed > k_maxShadowTiles)
                {
                    continue;
                }

                const math::Vec3 position = candidate.point != nullptr ? candidate.point->position
                    : candidate.spot->position;
                const float range = candidate.point != nullptr ? candidate.point->range : candidate.spot->range;

                for (uint32_t face = 0; face < needed; ++face)
                {
                    prepared.tiles[tile + face] = candidate.point != nullptr
                        ? fitPointShadowFace(*candidate.point, face, tile + face, view.shadowAtlasWidth)
                        : fitSpotShadow(*candidate.spot, tile + face, view.shadowAtlasWidth);
                    prepared.casterSpheres[tile + face - k_sunShadowTiles] = {position.x, position.y, position.z, range};
                    store(prepared.lighting.shadowTileDepth[tile + face], math::Vec3{0.0f, k_localShadowNear, range},
                        1.0f / std::max(prepared.tiles[tile + face].texelSize, 1e-6f));
                }

                GpuLocalLight& gpu = prepared.lighting.lights[candidate.slot];
                gpu.cosInnerKind[2] = static_cast<float>(tile);
                gpu.cosInnerKind[3] = shadows.localNormalBias * prepared.tiles[tile].texelSize;
                tile += needed;
                ++prepared.shadowedLocalLights;
            }

            prepared.localTileCount = tile - k_sunShadowTiles;
        }

        void packShadows(PreparedLighting& prepared, const FrameLights& lights, const RenderSettings& settings,
            const std::optional<DirectionalLight>& sun, const LightingView& view)
        {
            GpuFrameLighting& gpu = prepared.lighting;
            const ShadowSettings& shadows = settings.shadows;
            store(gpu.shadowParams, math::Vec3{0.0f, 0.0f, shadows.normalBias},
                shadows.debugCascades ? 1.0f : 0.0f);
            store(gpu.localShadowParams, math::Vec3{std::clamp(shadows.localFilterRadius, 0.5f, 3.0f), 0.0f, 0.0f},
                0.0f);
            store(gpu.cascadeTexelSize, math::Vec3{0.0f, 0.0f, 0.0f}, 0.0f);

            const SoftShadowSettings& soft = settings.softShadows;
            store(gpu.shadowSoftness, math::Vec3{soft.enabled ? 1.0f : 0.0f,
                static_cast<float>(std::clamp(soft.blockerTaps, 4u, 32u)),
                static_cast<float>(std::clamp(soft.filterTaps, 4u, 32u))}, std::max(soft.sunSize, 0.0f));
            store(gpu.localShadowSoftness, math::Vec3{std::max(soft.localRadius, 0.0f),
                std::clamp(soft.maxPenumbraTexels, 1.0f, 32.0f), 0.0f}, 0.0f);

            for (uint32_t tile = 0; tile < k_maxShadowTiles; ++tile)
            {
                const math::Vec4 rect = shadowTileRect(tile);
                store(gpu.shadowTileRects[tile], math::Vec3{rect.x, rect.y, rect.z}, rect.w);
            }

            if (!shadows.enabled)
            {
                return;
            }

            if (settings.lighting.localLights && shadows.localLights)
            {
                packLocalShadows(prepared, lights, shadows, view);
            }

            if (sun && sun->castsShadows)
            {
                const uint32_t count = std::clamp(shadows.cascadeCount, 1u, k_sunShadowTiles);
                prepared.sunTravel = math::normalize(sun->direction);
                fitSunCascades(view.camera, view.aspectRatio, prepared.sunTravel, shadows, count,
                    view.shadowAtlasWidth, prepared.tiles);
                prepared.cascadeCount = count;
                gpu.shadowParams[0] = static_cast<float>(count);

                for (uint32_t cascade = 0; cascade < count; ++cascade)
                {
                    gpu.cascadeTexelSize[cascade] = prepared.tiles[cascade].texelSize;
                    const math::Mat4& viewProjection = prepared.tiles[cascade].viewProjection;
                    const math::Vec4 start = viewProjection * math::Vec4{0.0f, 0.0f, 0.0f, 1.0f};
                    const math::Vec4 end = viewProjection * math::Vec4{prepared.sunTravel.x, prepared.sunTravel.y,
                        prepared.sunTravel.z, 1.0f};
                    const float depthPerMetre = std::max(std::abs(end.z - start.z), 1e-6f);
                    store(gpu.shadowTileDepth[cascade], math::Vec3{1.0f, 1.0f / depthPerMetre, 0.0f},
                        1.0f / std::max(prepared.tiles[cascade].texelSize, 1e-6f));
                }
            }

            for (uint32_t tile = 0; tile < k_maxShadowTiles; ++tile)
            {
                gpu.shadowTileViewProjection[tile] = prepared.tiles[tile].atlasViewProjection;
            }
        }

        float boxDistance(const math::Aabb& box, const math::Vec3& point)
        {
            const math::Vec3 below = box.min - point;
            const math::Vec3 above = point - box.max;
            const math::Vec3 outside{std::max({below.x, above.x, 0.0f}), std::max({below.y, above.y, 0.0f}),
                std::max({below.z, above.z, 0.0f})};

            return math::length(outside);
        }

        void packZones(GpuFrameLighting& gpu, const InteriorAmbientSettings& settings, const LightingView& view)
        {
            store(gpu.zoneParams, math::Vec3{0.0f, std::max(settings.blendDistance, 0.01f), settings.normalOffset},
                settings.debugView ? 1.0f : 0.0f);
            store(gpu.zoneLight, math::Vec3{settings.diffuseIntensity, settings.specularIntensity, 0.0f}, 0.0f);
            store(gpu.zoneFill, settings.fillColor * settings.fillIntensity, 0.0f);

            if (!settings.enabled)
            {
                return;
            }

            std::array<std::pair<float, uint32_t>, k_maxFrameAmbientZones> nearest{};
            uint32_t count = 0;

            for (uint32_t index = 0; index < view.zones.size(); ++index)
            {
                const float distance = boxDistance(view.zones[index].bounds, view.camera.position);
                const std::pair<float, uint32_t> entry{distance, index};

                if (count < k_maxFrameAmbientZones)
                {
                    nearest[count++] = entry;
                    std::push_heap(nearest.begin(), nearest.begin() + count);
                }
                else if (entry < nearest.front())
                {
                    std::pop_heap(nearest.begin(), nearest.begin() + count);
                    nearest[count - 1] = entry;
                    std::push_heap(nearest.begin(), nearest.begin() + count);
                }
            }

            std::sort_heap(nearest.begin(), nearest.begin() + count);

            for (uint32_t index = 0; index < count; ++index)
            {
                const math::Aabb& bounds = view.zones[nearest[index].second].bounds;
                store(gpu.zones[index].min, bounds.min, 0.0f);
                store(gpu.zones[index].max, bounds.max, 0.0f);
            }

            gpu.zoneParams[0] = static_cast<float>(count);
        }

        uint32_t fogLightMask(const GpuFrameLighting& gpu, uint32_t count, uint32_t maxLights, const math::Vec3& camera)
        {
            std::array<std::pair<float, uint32_t>, k_maxLocalLights> byDistance{};

            for (uint32_t index = 0; index < count; ++index)
            {
                const float* position = gpu.lights[index].positionRange;
                const math::Vec3 offset = math::Vec3{position[0], position[1], position[2]} - camera;
                byDistance[index] = {std::max(math::length(offset) - position[3], 0.0f), index};
            }

            std::sort(byDistance.begin(), byDistance.begin() + count);
            uint32_t mask = 0;

            for (uint32_t rank = 0; rank < std::min(count, maxLights); ++rank)
            {
                mask |= 1u << byDistance[rank].second;
            }

            return mask;
        }

        void packSurfaceDetail(GpuFrameLighting& gpu, const SurfaceDetailSettings& surface)
        {
            const float minSteps = static_cast<float>(std::clamp(surface.parallaxMinSteps, 4u, 64u));
            const float maxSteps = std::max(static_cast<float>(std::clamp(surface.parallaxMaxSteps, 4u, 64u)), minSteps);
            store(gpu.surfaceParallax, math::Vec3{surface.parallax ? std::max(surface.parallaxDistance, 0.0f) : 0.0f,
                minSteps, maxSteps}, surface.parallaxShadows ? 1.0f : 0.0f);
            store(gpu.surfaceDetail, math::Vec3{surface.detail ? std::max(surface.detailDistance, 0.0f) : 0.0f,
                std::clamp(surface.detailStrength, 0.0f, 2.0f), 0.0f}, 0.0f);
        }

        void packWetness(GpuFrameLighting& gpu, const WetnessSettings& wetness)
        {
            const float amount = wetness.enabled ? std::clamp(wetness.amount, 0.0f, 1.0f) : 0.0f;
            store(gpu.wetness, math::Vec3{amount, std::clamp(wetness.puddles, 0.0f, 1.0f),
                1.0f / std::max(wetness.puddleSize, 0.1f)}, wetness.rippleTime);
            store(gpu.wetnessShading, math::Vec3{std::clamp(wetness.darkening, 0.0f, 1.0f),
                std::clamp(wetness.glossiness, 0.0f, 1.0f), std::clamp(wetness.ripples, 0.0f, 1.0f)},
                wetness.debugView ? 1.0f : 0.0f);
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

    bool PreparedLighting::isTileActive(uint32_t tile) const
    {
        return tile < cascadeCount || (tile >= k_sunShadowTiles && tile < k_sunShadowTiles + localTileCount);
    }

    PreparedLighting prepareLighting(const FrameLights& lights, const RenderSettings& settings,
        const LightingView& view)
    {
        PreparedLighting prepared{};
        GpuFrameLighting& gpu = prepared.lighting;

        store(gpu.ambient, settings.lighting.ambientColor * settings.lighting.ambientIntensity, 0.0f);
        packSurfaceDetail(gpu, settings.surfaceDetail);
        packWetness(gpu, settings.wetness);

        const std::optional<DirectionalLight> skySun = frameSun(lights, settings);
        const math::Vec3 skySunDirection = skySun ? math::normalize(skySun->direction)
            : sunTravelDirection(settings.sky.sunAzimuth, settings.sky.sunElevation);
        const math::Vec3 skySunRadiance = skySun ? skySun->color * skySun->intensity : math::Vec3{0.0f, 0.0f, 0.0f};
        const std::optional<DirectionalLight> sun = lightningSun(skySun, settings);
        const math::Vec3 sunDirection = sun ? math::normalize(sun->direction) : skySunDirection;
        const math::Vec3 sunRadiance = sun ? sun->color * sun->intensity : math::Vec3{0.0f, 0.0f, 0.0f};

        prepared.hasSun = sun.has_value();
        store(gpu.sunDirection, sunDirection, sun ? 1.0f : 0.0f);
        store(gpu.sunRadiance, sunRadiance, 0.0f);
        store(gpu.skySunDirection, skySunDirection, 0.0f);
        store(gpu.skySunRadiance, skySunRadiance, 0.0f);
        packSky(gpu, settings, skySunDirection, skySunRadiance);

        const FogSettings& fog = settings.fog;
        store(gpu.fogColor, fog.color, fog.density);
        store(gpu.fogParams, math::Vec3{fog.start, fog.enabled ? 1.0f : 0.0f, fog.maxAmount},
            settings.sky.enabled ? std::clamp(fog.skyBlend, 0.0f, 1.0f) : 0.0f);

        uint32_t count = 0;

        if (settings.lighting.localLights)
        {
            for (const PointLight& point : lights.points)
            {
                packPoint(gpu.lights[count++], point, settings.lightShapes);
            }

            for (const SpotLight& spot : lights.spots)
            {
                packSpot(gpu.lights[count++], spot, settings.lightShapes);
            }
        }

        gpu.counts[0] = count;
        gpu.counts[1] = settings.lighting.specular ? 1u : 0u;
        gpu.counts[2] = fogLightMask(gpu, count, settings.volumetricFog.maxLights, view.camera.position);
        gpu.counts[3] = settings.surfaceDetail.parallaxShadows && settings.surfaceDetail.parallaxLocalShadows ? 1u : 0u;

        packShadows(prepared, lights, settings, sun, view);
        packZones(gpu, settings.interiorAmbient, view);

        return prepared;
    }
} //namespace lockstep::render
