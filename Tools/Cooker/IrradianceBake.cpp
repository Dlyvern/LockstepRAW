#include "IrradianceBake.hpp"

#include "BakeScene.hpp"
#include "IrradianceGrid.hpp"
#include "ParallelFor.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace lockstep::cooker
{
    namespace
    {
        constexpr std::string_view k_interiorZonePrefix{"Zone_Interior_"};
        constexpr math::Aabb k_unitCube{.min = {-1.0f, -1.0f, -1.0f}, .max = {1.0f, 1.0f, 1.0f}};
        constexpr float k_invalidBackFaceShare{0.25f};
        constexpr float k_evaluationOffset{0.3f};
        constexpr float k_infinity{1.0e30f};
        constexpr uint64_t k_maxStoredHitBytes{1536ull * 1024ull * 1024ull};
        constexpr double k_bytesPerMegabyte{1024.0 * 1024.0};

        using Clock = std::chrono::steady_clock;

        enum class HitKind : uint32_t
        {
            Miss = 0,
            Dark,
            Lit
        };

        struct HitRecord
        {
            math::Vec3 position{0.0f, 0.0f, 0.0f};
            math::Vec3 normal{0.0f, 0.0f, 0.0f};
            math::Vec3 local{0.0f, 0.0f, 0.0f};
            float sun{0.0f};
            float sky{0.0f};
            uint32_t surface{0};
            HitKind kind{HitKind::Miss};
            bool isBackFace{false};
        };

        struct ProbeRadiance
        {
            math::Vec3 local{0.0f, 0.0f, 0.0f};
            math::Vec3 sun{0.0f, 0.0f, 0.0f};
            math::Vec3 sky{0.0f, 0.0f, 0.0f};
        };

        struct BakeContext
        {
            const BakeScene& scene;
            const IrradianceGrid& grid;
            std::vector<math::Vec3> directions;
            float evaluationOffset{0.0f};
            bool storesHits{false};
            std::vector<HitRecord> hits;
        };

        double secondsSince(Clock::time_point start)
        {
            return std::chrono::duration<double>(Clock::now() - start).count();
        }

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

        std::vector<math::Vec3> fibonacciDirections(uint32_t count)
        {
            const float goldenAngle = math::k_pi * (3.0f - std::sqrt(5.0f));
            std::vector<math::Vec3> directions(count);

            for (uint32_t index = 0; index < count; ++index)
            {
                const float z = 1.0f - (2.0f * static_cast<float>(index) + 1.0f) / static_cast<float>(count);
                const float radius = std::sqrt(std::max(1.0f - z * z, 0.0f));
                const float angle = goldenAngle * static_cast<float>(index);
                directions[index] = math::Vec3{radius * std::cos(angle), radius * std::sin(angle), z};
            }

            return directions;
        }

        std::array<math::Vec3, 3> probeRotation(uint32_t probe)
        {
            const uint64_t bits = mixBits(static_cast<uint64_t>(probe) + 0x2545F4914F6CDD1Dull);
            const float u1 = unitFloat(bits);
            const float u2 = unitFloat(mixBits(bits));
            const float u3 = unitFloat(mixBits(bits + 1));
            const float low = std::sqrt(1.0f - u1);
            const float high = std::sqrt(u1);
            const float x = low * std::sin(2.0f * math::k_pi * u2);
            const float y = low * std::cos(2.0f * math::k_pi * u2);
            const float z = high * std::sin(2.0f * math::k_pi * u3);
            const float w = high * std::cos(2.0f * math::k_pi * u3);

            return {math::Vec3{1.0f - 2.0f * (y * y + z * z), 2.0f * (x * y + w * z), 2.0f * (x * z - w * y)},
                math::Vec3{2.0f * (x * y - w * z), 1.0f - 2.0f * (x * x + z * z), 2.0f * (y * z + w * x)},
                math::Vec3{2.0f * (x * z + w * y), 2.0f * (y * z - w * x), 1.0f - 2.0f * (x * x + y * y)}};
        }

        HitRecord traceRay(const BakeContext& context, const math::Vec3& origin, const math::Vec3& direction,
            uint64_t seed)
        {
            const std::optional<RayHit> hit = context.scene.tracer().closestHit(
                Ray{.origin = origin, .direction = direction, .maxDistance = k_infinity, .seed = seed});

            if (!hit)
            {
                return HitRecord{};
            }

            const BakeSurface& surface = context.scene.surface(hit->surface);
            HitRecord record{.position = {}, .normal = {}, .local = {}, .sun = 0.0f, .sky = 0.0f,
                .surface = hit->surface, .kind = HitKind::Dark, .isBackFace = hit->isBackFace};

            if (hit->isBackFace && !surface.doubleSided)
            {
                return record;
            }

            const math::Vec3 normal = hit->isBackFace ? hit->normal * -1.0f : hit->normal;
            const math::Vec3 point = origin + direction * hit->distance;
            const DirectLight direct = context.scene.directLight(point, normal, mixBits(seed));
            record.kind = HitKind::Lit;
            record.normal = normal;
            record.position = point + normal * context.evaluationOffset;
            record.local = surface.emissive + surface.albedo * direct.local;
            record.sun = direct.sun;
            record.sky = direct.sky;

            return record;
        }

        struct ProbeTrace
        {
            uint32_t backFaces{0};
            uint32_t escaped{0};
        };

        ProbeTrace traceProbe(BakeContext& context, uint32_t probe, const IrradianceGrid* previous,
            render::IrradianceProbe& result)
        {
            const uint32_t rayCount = static_cast<uint32_t>(context.directions.size());
            const std::array<math::Vec3, 3> rotation = probeRotation(probe);
            const math::Vec3 origin = context.grid.probePosition(probe);
            ProbeAccumulator accumulator(rayCount);
            ProbeTrace trace{};

            for (uint32_t ray = 0; ray < rayCount; ++ray)
            {
                const math::Vec3& fixed = context.directions[ray];
                const math::Vec3 direction = math::normalize(rotation[0] * fixed.x + rotation[1] * fixed.y
                    + rotation[2] * fixed.z);
                const std::size_t slot = static_cast<std::size_t>(probe) * rayCount + ray;
                const HitRecord record = context.storesHits && previous != nullptr ? context.hits[slot]
                    : traceRay(context, origin, direction, static_cast<uint64_t>(slot) + 1);

                if (context.storesHits && previous == nullptr)
                {
                    context.hits[slot] = record;
                }

                if (record.kind == HitKind::Miss)
                {
                    ++trace.escaped;
                    accumulator.addSky(direction);
                    continue;
                }

                if (record.isBackFace)
                {
                    ++trace.backFaces;
                }

                if (record.kind == HitKind::Dark)
                {
                    continue;
                }

                const BakeSurface& surface = context.scene.surface(record.surface);
                ProbeRadiance radiance{.local = record.local, .sun = surface.albedo * record.sun,
                    .sky = surface.albedo * record.sky};

                if (previous != nullptr)
                {
                    const IndirectLight bounce = previous->evaluate(record.position, record.normal);
                    radiance.local += surface.albedo * bounce.local;
                    radiance.sun += surface.albedo * bounce.sun;
                    radiance.sky += surface.albedo * bounce.sky;
                }

                accumulator.add(direction, radiance.local, radiance.sun, radiance.sky);
            }

            result = accumulator.probe();

            return trace;
        }
    } //namespace

    math::Vec3 sunTravelDirection(float azimuth, float elevation)
    {
        const math::Vec3 toSun{std::sin(azimuth) * std::cos(elevation), std::sin(elevation),
            -std::cos(azimuth) * std::cos(elevation)};

        return toSun * -1.0f;
    }

    Result<render::IrradianceVolume> bakeIrradiance(const SourceScene& scene, std::span<const TextureMean> means,
        const IrradianceBakeOptions& options, uint32_t threads)
    {
        const Clock::time_point start = Clock::now();
        std::optional<math::Vec3> sunTravel;

        if (options.sun)
        {
            sunTravel = sunTravelDirection(math::radians(options.sun->x), math::radians(options.sun->y));
        }

        auto bakeScene = BakeScene::create(scene, means, sunTravel);

        if (!bakeScene)
        {
            return bakeScene.error();
        }

        math::Aabb zones{};
        uint32_t zoneCount = 0;

        for (const assets::SceneNode& node : scene.nodes)
        {
            if (std::string_view(node.name).starts_with(k_interiorZonePrefix))
            {
                math::expand(zones, math::transformBounds(k_unitCube, node.transform));
                ++zoneCount;
            }
        }

        const math::Aabb covered = zoneCount > 0 ? zones : scene.bounds;
        IrradianceGrid grid = IrradianceGrid::covering(covered, options.spacing);
        const BakeSceneReport& report = bakeScene.value().report();

        std::cout << std::fixed << std::setprecision(2) << "  gi: " << bakeScene.value().tracer().triangleCount()
            << " triangles from " << report.instances << " instances (" << report.skippedInstances << " skipped), "
            << report.lights << " lights (" << report.tubeLights << " tubes, " << report.skippedLights
            << " skipped), BVH in " << secondsSince(start) << " s\n";
        std::cout << "  gi: volume over " << (zoneCount > 0 ? std::to_string(zoneCount) + " interior zones"
            : std::string("the scene bounds")) << ": " << grid.countX() << " x " << grid.countY() << " x "
            << grid.countZ() << " probes, spacing " << grid.spacing().x << " m";

        if (grid.spacing().x > options.spacing)
        {
            std::cout << " (grown from " << options.spacing << " m to fit the probe limit)";
        }

        std::cout << '\n';

        const uint32_t probeCount = grid.probeCount();
        const uint64_t hitBytes = static_cast<uint64_t>(probeCount) * options.rays * sizeof(HitRecord);
        BakeContext context{.scene = bakeScene.value(), .grid = grid, .directions = fibonacciDirections(options.rays),
            .evaluationOffset = k_evaluationOffset * std::min({grid.spacing().x, grid.spacing().y, grid.spacing().z}),
            .storesHits = hitBytes <= k_maxStoredHitBytes, .hits = {}};

        if (context.storesHits)
        {
            context.hits.resize(static_cast<std::size_t>(probeCount) * options.rays);
        }

        std::vector<render::IrradianceProbe> probes(probeCount);
        std::vector<ProbeTrace> traces(probeCount);
        std::optional<IrradianceGrid> previous;

        for (uint32_t bounce = 0; bounce < options.bounces; ++bounce)
        {
            const Clock::time_point passStart = Clock::now();

            parallelFor(probeCount, threads, [&](std::size_t probe)
            {
                const ProbeTrace trace = traceProbe(context, static_cast<uint32_t>(probe),
                    previous ? &*previous : nullptr, probes[probe]);

                if (!previous)
                {
                    traces[probe] = trace;
                }
            });

            if (!previous)
            {
                std::vector<bool> valid(probeCount);

                for (uint32_t probe = 0; probe < probeCount; ++probe)
                {
                    valid[probe] = static_cast<float>(traces[probe].backFaces)
                        <= k_invalidBackFaceShare * static_cast<float>(options.rays);
                }

                grid.setValidity(std::move(valid));
            }

            grid.setProbes(probes);
            const uint32_t unfilled = grid.fillInvalid();
            previous = grid;

            std::cout << "  gi: bounce " << (bounce + 1) << " in " << secondsSince(passStart) << " s";

            if (bounce == 0)
            {
                std::cout << " (" << (context.storesHits ? "hits kept, " : "hits retraced per bounce, ")
                    << static_cast<double>(hitBytes) / k_bytesPerMegabyte << " MiB); " << grid.invalidCount()
                    << " probes inside geometry, " << unfilled << " left unfilled";
            }

            std::cout << '\n';
        }

        render::IrradianceVolume volume = grid.volume();
        volume.sunTravel = sunTravel.value_or(math::Vec3{0.0f, 0.0f, 0.0f});

        std::cout << "  gi: baked " << probeCount << " probes (" << options.rays << " rays, " << options.bounces
            << " bounces) in " << secondsSince(start) << " s\n" << std::defaultfloat;

        return volume;
    }
} //namespace lockstep::cooker
