#include "RayTracer.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace lockstep::cooker
{
    namespace
    {
        constexpr uint32_t k_binCount{16};
        constexpr uint32_t k_leafSize{2};
        constexpr uint32_t k_maxLeafSize{16};
        constexpr std::size_t k_stackSize{96};
        constexpr float k_minDistance{1.0e-5f};
        constexpr float k_minDeterminant{1.0e-14f};
        constexpr float k_tinyDirection{1.0e-20f};
        constexpr float k_traversalCost{1.0f};

        struct BuildTriangle
        {
            math::Aabb bounds{};
            math::Vec3 centroid{0.0f, 0.0f, 0.0f};
        };

        struct Bin
        {
            math::Aabb bounds{};
            uint32_t count{0};
        };

        struct Split
        {
            uint32_t axis{0};
            uint32_t bin{0};
            float cost{std::numeric_limits<float>::max()};
        };

        float surfaceArea(const math::Aabb& box)
        {
            if (math::isEmpty(box))
            {
                return 0.0f;
            }

            const math::Vec3 size = box.max - box.min;
            return 2.0f * (size.x * size.y + size.y * size.z + size.z * size.x);
        }

        uint32_t binOf(float value, float minimum, float scale)
        {
            const auto bin = static_cast<int64_t>((value - minimum) * scale);
            return static_cast<uint32_t>(std::clamp<int64_t>(bin, 0, k_binCount - 1));
        }

        Split bestSplit(std::span<const uint32_t> order, const std::vector<BuildTriangle>& triangles,
            const math::Aabb& centroids)
        {
            Split best{};

            for (uint32_t axis = 0; axis < 3; ++axis)
            {
                const float extent = centroids.max[axis] - centroids.min[axis];

                if (!(extent > 0.0f))
                {
                    continue;
                }

                const float scale = static_cast<float>(k_binCount) / extent;
                std::array<Bin, k_binCount> bins{};

                for (const uint32_t index : order)
                {
                    Bin& bin = bins[binOf(triangles[index].centroid[axis], centroids.min[axis], scale)];
                    math::expand(bin.bounds, triangles[index].bounds);
                    ++bin.count;
                }

                std::array<float, k_binCount> rightCost{};
                math::Aabb right{};
                uint32_t rightCount = 0;

                for (uint32_t bin = k_binCount - 1; bin > 0; --bin)
                {
                    math::expand(right, bins[bin].bounds);
                    rightCount += bins[bin].count;
                    rightCost[bin] = surfaceArea(right) * static_cast<float>(rightCount);
                }

                math::Aabb left{};
                uint32_t leftCount = 0;

                for (uint32_t bin = 0; bin + 1 < k_binCount; ++bin)
                {
                    math::expand(left, bins[bin].bounds);
                    leftCount += bins[bin].count;

                    if (leftCount == 0 || leftCount == order.size())
                    {
                        continue;
                    }

                    const float cost = surfaceArea(left) * static_cast<float>(leftCount) + rightCost[bin + 1];

                    if (cost < best.cost)
                    {
                        best = Split{.axis = axis, .bin = bin, .cost = cost};
                    }
                }
            }

            return best;
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

        bool passesCoverage(float coverage, uint64_t seed, uint32_t triangle)
        {
            if (coverage >= 1.0f)
            {
                return true;
            }

            const uint64_t bits = mixBits(seed ^ (static_cast<uint64_t>(triangle) * 0x9E3779B97F4A7C15ull));
            return static_cast<float>(bits >> 40) * (1.0f / 16777216.0f) < coverage;
        }

        float boxEntry(const math::Aabb& box, const math::Vec3& origin, const math::Vec3& inverse, float maxDistance)
        {
            float nearest = 0.0f;
            float farthest = maxDistance;

            for (uint32_t axis = 0; axis < 3; ++axis)
            {
                const float first = (box.min[axis] - origin[axis]) * inverse[axis];
                const float second = (box.max[axis] - origin[axis]) * inverse[axis];
                nearest = std::max(nearest, std::min(first, second));
                farthest = std::min(farthest, std::max(first, second));
            }

            return nearest <= farthest ? nearest : std::numeric_limits<float>::infinity();
        }
    } //namespace

    Result<RayTracer> RayTracer::create(std::span<const RayTriangle> triangles, std::vector<RaySurface> surfaces)
    {
        RayTracer tracer;
        tracer.m_surfaces = std::move(surfaces);
        std::vector<PackedTriangle> packed;
        std::vector<BuildTriangle> build;
        packed.reserve(triangles.size());
        build.reserve(triangles.size());

        for (std::size_t index = 0; index < triangles.size(); ++index)
        {
            const RayTriangle& triangle = triangles[index];

            if (triangle.surface >= tracer.m_surfaces.size())
            {
                return Error{ErrorCode::InvalidArgument, "ray tracer: triangle surface out of range"};
            }

            const math::Vec3 edge1 = triangle.corners[1] - triangle.corners[0];
            const math::Vec3 edge2 = triangle.corners[2] - triangle.corners[0];

            if (!(math::lengthSquared(math::cross(edge1, edge2)) > 1.0e-14f))
            {
                continue;
            }

            BuildTriangle info{};

            for (const math::Vec3& corner : triangle.corners)
            {
                math::expand(info.bounds, corner);
            }

            info.centroid = (triangle.corners[0] + triangle.corners[1] + triangle.corners[2]) * (1.0f / 3.0f);
            build.push_back(info);
            packed.push_back(PackedTriangle{.corner = triangle.corners[0], .edge1 = edge1, .edge2 = edge2,
                .surface = triangle.surface, .id = static_cast<uint32_t>(index)});
        }

        if (packed.empty())
        {
            return Error{ErrorCode::InvalidArgument, "ray tracer: no triangles"};
        }

        std::vector<uint32_t> order(packed.size());

        for (uint32_t index = 0; index < order.size(); ++index)
        {
            order[index] = index;
        }

        tracer.m_nodes.reserve(packed.size() * 2);
        tracer.m_nodes.push_back(Node{.bounds = {}, .first = 0, .count = static_cast<uint32_t>(order.size())});
        std::vector<uint32_t> pending{0};

        while (!pending.empty())
        {
            const uint32_t nodeIndex = pending.back();
            pending.pop_back();
            const uint32_t begin = tracer.m_nodes[nodeIndex].first;
            const uint32_t count = tracer.m_nodes[nodeIndex].count;
            const std::span<uint32_t> range(order.data() + begin, count);
            math::Aabb bounds{};
            math::Aabb centroids{};

            for (const uint32_t index : range)
            {
                math::expand(bounds, build[index].bounds);
                math::expand(centroids, build[index].centroid);
            }

            tracer.m_nodes[nodeIndex].bounds = bounds;

            if (count <= k_leafSize)
            {
                continue;
            }

            const Split split = bestSplit(range, build, centroids);
            const float leafCost = surfaceArea(bounds) * static_cast<float>(count);

            if (split.cost == std::numeric_limits<float>::max()
                || (split.cost + k_traversalCost * surfaceArea(bounds) >= leafCost && count <= k_maxLeafSize))
            {
                continue;
            }

            const float scale = static_cast<float>(k_binCount) / (centroids.max[split.axis] - centroids.min[split.axis]);
            const auto middle = std::partition(range.begin(), range.end(), [&](uint32_t index)
            {
                return binOf(build[index].centroid[split.axis], centroids.min[split.axis], scale) <= split.bin;
            });
            const auto leftCount = static_cast<uint32_t>(middle - range.begin());

            if (leftCount == 0 || leftCount == count)
            {
                continue;
            }

            const auto left = static_cast<uint32_t>(tracer.m_nodes.size());
            tracer.m_nodes.push_back(Node{.bounds = {}, .first = begin, .count = leftCount});
            tracer.m_nodes.push_back(Node{.bounds = {}, .first = begin + leftCount, .count = count - leftCount});
            tracer.m_nodes[nodeIndex].first = left;
            tracer.m_nodes[nodeIndex].count = 0;
            pending.push_back(left + 1);
            pending.push_back(left);
        }

        tracer.m_triangles.reserve(order.size());

        for (const uint32_t index : order)
        {
            tracer.m_triangles.push_back(packed[index]);
        }

        return tracer;
    }

    template<bool k_anyHit>
    bool RayTracer::traverse(const Ray& ray, RayHit* hit) const
    {
        math::Vec3 inverse{};

        for (uint32_t axis = 0; axis < 3; ++axis)
        {
            const float component = ray.direction[axis];
            const float safe = std::abs(component) > k_tinyDirection ? component
                : std::copysign(k_tinyDirection, component);
            inverse[axis] = 1.0f / safe;
        }

        float maxDistance = ray.maxDistance;
        bool found = false;
        std::array<uint32_t, k_stackSize> stack{};
        std::size_t depth = 0;

        if (boxEntry(m_nodes[0].bounds, ray.origin, inverse, maxDistance) > maxDistance)
        {
            return false;
        }

        stack[depth++] = 0;

        while (depth > 0)
        {
            const Node& node = m_nodes[stack[--depth]];

            if (node.count == 0)
            {
                const float nearLeft = boxEntry(m_nodes[node.first].bounds, ray.origin, inverse, maxDistance);
                const float nearRight = boxEntry(m_nodes[node.first + 1].bounds, ray.origin, inverse, maxDistance);
                const bool leftFirst = nearLeft <= nearRight;
                const float nearFirst = leftFirst ? nearLeft : nearRight;
                const float nearSecond = leftFirst ? nearRight : nearLeft;

                if (nearSecond <= maxDistance && depth < k_stackSize)
                {
                    stack[depth++] = leftFirst ? node.first + 1 : node.first;
                }

                if (nearFirst <= maxDistance && depth < k_stackSize)
                {
                    stack[depth++] = leftFirst ? node.first : node.first + 1;
                }

                continue;
            }

            for (uint32_t index = node.first; index < node.first + node.count; ++index)
            {
                const PackedTriangle& triangle = m_triangles[index];
                const math::Vec3 p = math::cross(ray.direction, triangle.edge2);
                const float determinant = math::dot(triangle.edge1, p);

                if (std::abs(determinant) < k_minDeterminant)
                {
                    continue;
                }

                const float inverseDeterminant = 1.0f / determinant;
                const math::Vec3 s = ray.origin - triangle.corner;
                const float u = math::dot(s, p) * inverseDeterminant;

                if (u < 0.0f || u > 1.0f)
                {
                    continue;
                }

                const math::Vec3 q = math::cross(s, triangle.edge1);
                const float v = math::dot(ray.direction, q) * inverseDeterminant;

                if (v < 0.0f || u + v > 1.0f)
                {
                    continue;
                }

                const float distance = math::dot(triangle.edge2, q) * inverseDeterminant;

                if (distance <= k_minDistance || distance >= maxDistance)
                {
                    continue;
                }

                const RaySurface& surface = m_surfaces[triangle.surface];

                if ((k_anyHit && !surface.blocksShadows) || !passesCoverage(surface.coverage, ray.seed, triangle.id))
                {
                    continue;
                }

                if constexpr (k_anyHit)
                {
                    return true;
                }

                found = true;
                maxDistance = distance;
                hit->distance = distance;
                hit->surface = triangle.surface;
                hit->normal = math::normalize(math::cross(triangle.edge1, triangle.edge2));
                hit->isBackFace = determinant < 0.0f;
            }
        }

        return found;
    }

    std::optional<RayHit> RayTracer::closestHit(const Ray& ray) const
    {
        RayHit hit{};

        if (!traverse<false>(ray, &hit))
        {
            return std::nullopt;
        }

        return hit;
    }

    bool RayTracer::isOccluded(const Ray& ray) const
    {
        return traverse<true>(ray, nullptr);
    }

    std::size_t RayTracer::triangleCount() const noexcept
    {
        return m_triangles.size();
    }

    math::Aabb RayTracer::bounds() const noexcept
    {
        return m_nodes.front().bounds;
    }
} //namespace lockstep::cooker
