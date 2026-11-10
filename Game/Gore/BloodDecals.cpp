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

    void drawDecals(lockstep::render::Renderer& renderer, const BloodDecals& decals, lockstep::render::MeshHandle quad)
    {
        for (const BloodDecal& decal : decals.decals)
        {
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
