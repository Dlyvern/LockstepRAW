#include "Gore/BloodDecals.hpp"

#include <Lockstep/Core/Transform.hpp>
#include <Lockstep/Physics/PhysicsWorld.hpp>
#include <Lockstep/Render/Renderer.hpp>

#include <algorithm>
#include <array>
#include <cmath>

namespace game
{
    namespace
    {
        constexpr math::Vec3 k_up{0.0f, 1.0f, 0.0f};
        constexpr math::Vec3 k_sideAxis{1.0f, 0.0f, 0.0f};
        constexpr float k_lift{0.004f};
        constexpr float k_parallel{0.9999f};
        constexpr float k_startShare{0.12f};
        constexpr float k_dripLimit{0.25f};
        constexpr math::Vec3 k_lengthAxis{1.0f, 0.0f, 0.0f};
        constexpr math::Vec3 k_widthAxis{0.0f, 0.0f, 1.0f};
        constexpr float k_footprintEdge{0.42f};
        constexpr float k_footprintCorner{0.3f};
        constexpr float k_probeLift{0.06f};
        constexpr float k_probeDepth{0.025f};
        constexpr float k_minimumAlignment{0.85f};
        constexpr float k_shrink{0.6f};
        constexpr float k_maxRunNormalY{0.6f};
        constexpr float k_runWidth{0.25f};
        constexpr float k_runAbove{0.08f};
        constexpr float k_minRun{0.15f};
        constexpr math::Vec3 k_runAxis{0.0f, 0.0f, 1.0f};
        constexpr std::array<std::array<float, 2>, 8> k_footprint{{{k_footprintEdge, 0.0f}, {-k_footprintEdge, 0.0f},
            {0.0f, k_footprintEdge}, {0.0f, -k_footprintEdge}, {k_footprintCorner, k_footprintCorner},
            {-k_footprintCorner, k_footprintCorner}, {k_footprintCorner, -k_footprintCorner},
            {-k_footprintCorner, -k_footprintCorner}}};

        bool isFlatFootprint(const lockstep::physics::PhysicsWorld& physics, const BloodDecal& decal, float size)
        {
            const math::Vec3 normal = decal.rotation * k_up;
            const math::Vec3 length = decal.rotation * k_lengthAxis * (size * decal.stretch);
            const math::Vec3 width = decal.rotation * k_widthAxis * size;
            const math::Vec3 surface = decal.position - normal * k_lift;

            return std::all_of(k_footprint.begin(), k_footprint.end(), [&](const std::array<float, 2>& sample)
            {
                const math::Vec3 origin = surface + length * sample[0] + width * sample[1] + normal * k_probeLift;
                const auto hit = physics.raycast(origin, -normal, k_probeLift + k_probeDepth);
                return hit && !hit->body.isNull() && math::dot(hit->normal, normal) >= k_minimumAlignment
                    && std::abs(hit->distance - k_probeLift) <= k_probeDepth;
            });
        }
    } //namespace

    math::Quat rotationBetween(const math::Vec3& from, const math::Vec3& to)
    {
        const float cosine = math::clamp(math::dot(from, to), -1.0f, 1.0f);

        if (cosine > k_parallel)
        {
            return math::Quat{};
        }

        if (cosine < -k_parallel)
        {
            const math::Vec3 helper = std::abs(from.x) < 0.9f ? k_sideAxis : k_up;
            return math::angleAxis(math::k_pi, math::normalize(math::cross(from, helper)));
        }

        return math::angleAxis(std::acos(cosine), math::normalize(math::cross(from, to)));
    }

    BloodDecal decalOnSurface(const math::Vec3& point, const math::Vec3& normal, float spin)
    {
        return BloodDecal{.position = point + normal * k_lift,
            .rotation = rotationBetween(k_up, normal) * math::angleAxis(spin, k_up)};
    }

    bool isPaintable(const BloodDecals& decals, const lockstep::physics::RaycastHit& hit)
    {
        return !hit.body.isNull()
            && std::find(decals.unpaintable.begin(), decals.unpaintable.end(), hit.body) == decals.unpaintable.end();
    }

    bool isRunningSurface(const math::Vec3& normal)
    {
        return std::abs(normal.y) < k_maxRunNormalY;
    }

    void addWallRun(BloodDecals& decals, const lockstep::physics::PhysicsWorld& physics,
        lockstep::render::MaterialHandle material, const math::Vec3& point, const math::Vec3& normal, float length,
        float seconds, uint32_t maxDecals)
    {
        const math::Vec3 downhill = -k_up + normal * math::dot(k_up, normal);

        if (!isRunningSurface(normal) || math::lengthSquared(downhill) < 1.0e-6f)
        {
            return;
        }

        const math::Vec3 down = math::normalize(downhill);
        const math::Quat lay = rotationBetween(k_up, normal);
        const math::Vec3 laidAxis = lay * k_runAxis;
        const float spin = std::atan2(math::dot(math::cross(laidAxis, down), normal), math::dot(laidAxis, down));
        BloodDecal run{.material = material,
            .position = point + normal * k_lift - down * (k_runAbove * length),
            .rotation = math::angleAxis(spin, normal) * lay,
            .size = length,
            .stretch = k_runWidth,
            .runSeconds = std::max(seconds, 0.01f)};
        BloodDecal middle = run;
        middle.position = run.position + down * (0.5f * length);

        if (!fitDecal(physics, middle, k_minRun))
        {
            return;
        }

        run.size = middle.size;
        addDecal(decals, run, maxDecals);
    }

    bool fitDecal(const lockstep::physics::PhysicsWorld& physics, BloodDecal& decal, float minSize)
    {
        for (float size = decal.size; size >= minSize; size *= k_shrink)
        {
            if (isFlatFootprint(physics, decal, size))
            {
                decal.size = size;
                return true;
            }
        }

        return false;
    }

    void addDecal(BloodDecals& decals, const BloodDecal& decal, uint32_t maxDecals)
    {
        if (maxDecals == 0)
        {
            return;
        }

        while (decals.decals.size() >= maxDecals)
        {
            const auto drip = std::find_if(decals.decals.begin(), decals.decals.end(),
                [](const BloodDecal& old) { return old.growSeconds <= 0.0f && old.size < k_dripLimit; });
            decals.decals.erase(drip != decals.decals.end() ? drip : decals.decals.begin());
        }

        decals.decals.push_back(decal);
    }

    void updateDecals(BloodDecals& decals, float seconds)
    {
        for (BloodDecal& decal : decals.decals)
        {
            decal.age += seconds;
        }
    }

    void drawDecals(lockstep::render::Renderer& renderer, const BloodDecals& decals, lockstep::render::MeshHandle quad,
        std::span<const lockstep::render::MeshHandle> runQuads)
    {
        for (const BloodDecal& decal : decals.decals)
        {
            if (decal.runSeconds > 0.0f)
            {
                if (runQuads.empty())
                {
                    continue;
                }

                const float flow = std::min(decal.age / decal.runSeconds, 1.0f);
                const float eased = 1.0f - (1.0f - flow) * (1.0f - flow);
                const auto steps = static_cast<float>(runQuads.size());
                const auto step = static_cast<std::size_t>(std::clamp(std::ceil(eased * steps), 1.0f, steps)) - 1;
                renderer.submitDecal(runQuads[step], decal.material, math::Transform{
                    .position = decal.position + decal.rotation * k_runAxis * (0.5f * decal.size),
                    .rotation = decal.rotation,
                    .scale = math::Vec3{decal.size * decal.stretch, 1.0f, decal.size}});
                continue;
            }

            float grow = 1.0f;

            if (decal.growSeconds > 0.0f)
            {
                const float progress = std::min(decal.age / decal.growSeconds, 1.0f);
                grow = k_startShare + (1.0f - k_startShare) * std::sqrt(progress);
            }

            const float size = decal.size * grow;
            renderer.submitDecal(quad, decal.material, math::Transform{.position = decal.position,
                .rotation = decal.rotation,
                .scale = math::Vec3{size * decal.stretch, 1.0f, size}});
        }
    }
} //namespace game
