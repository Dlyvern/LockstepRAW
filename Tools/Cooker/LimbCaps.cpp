#include "LimbMesh.hpp"

#include <algorithm>
#include <cmath>
#include <map>
#include <utility>

namespace lockstep::cooker::limbmesh
{
    namespace
    {
        using anim::Limb;
        using anim::LimbMask;
        using Weights = std::map<uint32_t, float>;

        constexpr float k_recessShare{0.25f};
        constexpr float k_discCentreU{0.244f};
        constexpr float k_discCentreV{0.232f};
        constexpr float k_discRadius{0.2f};
        constexpr float k_woundStandShare{0.05f};
        constexpr float k_woundRadiusShare{1.0f};
        constexpr float k_minimumNormal{1.0e-12f};

        using DirectedEdge = std::pair<uint32_t, uint32_t>;

        std::vector<CapLoop> chainLoops(const std::vector<DirectedEdge>& edges, bool isFromInside)
        {
            std::multimap<uint32_t, std::size_t> starts;

            for (std::size_t index = 0; index < edges.size(); ++index)
            {
                starts.emplace(edges[index].first, index);
            }

            std::vector<bool> used(edges.size(), false);
            std::vector<CapLoop> loops;

            for (std::size_t first = 0; first < edges.size(); ++first)
            {
                if (used[first])
                {
                    continue;
                }

                CapLoop loop{.vertices = {}, .isClosed = false, .isFromInside = isFromInside};
                std::size_t edge = first;

                while (true)
                {
                    used[edge] = true;
                    loop.vertices.push_back(edges[edge].first);
                    const uint32_t end = edges[edge].second;

                    if (end == edges[first].first)
                    {
                        loop.isClosed = true;
                        break;
                    }

                    auto [begin, last] = starts.equal_range(end);
                    const auto next = std::find_if(begin, last, [&](const auto& entry) { return !used[entry.second]; });

                    if (next == last)
                    {
                        loop.vertices.push_back(end);
                        break;
                    }

                    edge = next->second;
                }

                if (loop.vertices.size() >= 3)
                {
                    loops.push_back(std::move(loop));
                }
            }

            return loops;
        }

        math::Vec3 loopCentroid(const assets::GltfSkinnedModel& model, const WeldedMesh& mesh, const CapLoop& loop)
        {
            math::Vec3 centroid{0.0f, 0.0f, 0.0f};

            for (const uint32_t welded : loop.vertices)
            {
                centroid += positionOf(sourceVertex(model, mesh.sources[welded])) / static_cast<float>(loop.vertices.size());
            }

            return centroid;
        }

        std::array<std::pair<uint32_t, float>, 4> strongest(const Weights& weights, uint32_t fallback)
        {
            std::vector<std::pair<uint32_t, float>> sorted(weights.begin(), weights.end());
            std::stable_sort(sorted.begin(), sorted.end(), [](const auto& a, const auto& b) { return a.second > b.second; });
            std::array<std::pair<uint32_t, float>, 4> result{};
            float total = 0.0f;

            for (std::size_t index = 0; index < result.size() && index < sorted.size(); ++index)
            {
                result[index] = sorted[index];
                total += sorted[index].second;
            }

            if (!(total > 0.0f))
            {
                return {{{fallback, 1.0f}, {0, 0.0f}, {0, 0.0f}, {0, 0.0f}}};
            }

            for (auto& [joint, weight] : result)
            {
                weight /= total;
            }

            return result;
        }
        void addSideWeights(const render::SkinnedPbrVertex& vertex, const std::vector<Limb>& jointLimbs,
            const CapSide& side, Weights& weights)
        {
            for (std::size_t influence = 0; influence < 4; ++influence)
            {
                const auto joint = static_cast<uint32_t>(vertex.joints[influence]);
                const bool isSideJoint = joint < jointLimbs.size()
                    && ((side.joints & anim::limbBit(jointLimbs[joint])) != 0);

                if (isSideJoint && vertex.weights[influence] > 0.0f)
                {
                    weights[joint] += vertex.weights[influence];
                }
            }
        }

        math::Quat rotationFromUp(const math::Vec3& direction)
        {
            const math::Vec3 up{0.0f, 1.0f, 0.0f};
            const float cosine = math::dot(up, direction);

            if (cosine < -0.9999f)
            {
                return math::angleAxis(math::k_pi, math::Vec3{1.0f, 0.0f, 0.0f});
            }

            const math::Vec3 axis = math::cross(up, direction);

            if (math::lengthSquared(axis) < 1.0e-12f)
            {
                return math::Quat{};
            }

            return math::angleAxis(std::acos(std::clamp(cosine, -1.0f, 1.0f)), math::normalize(axis));
        }
    } //namespace

    std::vector<CapLoop> cutLoops(const WeldedMesh& mesh, LimbMask inside)
    {
        const auto isInside = [inside](Limb limb) { return (inside & anim::limbBit(limb)) != 0; };
        std::vector<DirectedEdge> cuts;

        for (const Triangle& triangle : mesh.triangles)
        {
            if (!isInside(triangle.limb))
            {
                continue;
            }

            for (std::size_t corner = 0; corner < 3; ++corner)
            {
                const uint32_t from = triangle.welded[corner];
                const uint32_t to = triangle.welded[(corner + 1) % 3];
                const auto twins = mesh.edges.find({to, from});

                if (from == to || twins == mesh.edges.end())
                {
                    continue;
                }

                const bool hasInsideTwin = std::any_of(twins->second.begin(), twins->second.end(),
                    [&](uint32_t twin) { return isInside(mesh.triangles[twin].limb); });

                if (!hasInsideTwin)
                {
                    cuts.emplace_back(from, to);
                }
            }
        }

        return chainLoops(cuts, true);
    }

    std::optional<CapLoop> nearestOpenLoop(const assets::GltfSkinnedModel& model, const WeldedMesh& mesh,
        LimbMask inside, bool isInsideSide, const math::Vec3& joint, float reach)
    {
        std::vector<DirectedEdge> borders;

        for (const Triangle& triangle : mesh.triangles)
        {
            if (((inside & anim::limbBit(triangle.limb)) != 0) != isInsideSide)
            {
                continue;
            }

            for (std::size_t corner = 0; corner < 3; ++corner)
            {
                const uint32_t from = triangle.welded[corner];
                const uint32_t to = triangle.welded[(corner + 1) % 3];

                if (from != to && mesh.edges.find({to, from}) == mesh.edges.end())
                {
                    borders.emplace_back(from, to);
                }
            }
        }

        std::optional<CapLoop> nearest;
        float nearestDistance = reach;

        for (CapLoop& loop : chainLoops(borders, isInsideSide))
        {
            const float distance = math::length(loopCentroid(model, mesh, loop) - joint);

            if (distance < nearestDistance)
            {
                nearestDistance = distance;
                nearest = std::move(loop);
            }
        }

        return nearest;
    }

    void addCapLoop(const assets::GltfSkinnedModel& model, const WeldedMesh& mesh, const std::vector<Limb>& jointLimbs,
        const CapLoop& loop, const CapSide& side, assets::SkinnedGeometry& geometry)
    {
        const std::size_t count = loop.vertices.size();
        math::Vec3 centroid{0.0f, 0.0f, 0.0f};
        math::Vec3 newell{0.0f, 0.0f, 0.0f};

        for (std::size_t index = 0; index < count; ++index)
        {
            const math::Vec3 current = positionOf(sourceVertex(model, mesh.sources[loop.vertices[index]]));
            const math::Vec3 next = positionOf(sourceVertex(model, mesh.sources[loop.vertices[(index + 1) % count]]));
            centroid += current / static_cast<float>(count);
            newell += math::cross(current, next);
        }

        if (math::lengthSquared(newell) < k_minimumNormal)
        {
            return;
        }

        const bool closesEdgeSide = side.isInside == loop.isFromInside;
        const math::Vec3 normal = closesEdgeSide ? -math::normalize(newell) : math::normalize(newell);
        float radius = 0.0f;
        Weights centreWeights;

        for (const uint32_t welded : loop.vertices)
        {
            const render::SkinnedPbrVertex& vertex = sourceVertex(model, mesh.sources[welded]);
            radius += math::length(positionOf(vertex) - centroid) / static_cast<float>(count);
            addSideWeights(vertex, jointLimbs, side, centreWeights);
        }

        const math::Vec3 axis = std::abs(normal.y) < 0.9f ? math::Vec3{0.0f, 1.0f, 0.0f} : math::Vec3{1.0f, 0.0f, 0.0f};
        const math::Vec3 tangent = math::normalize(math::cross(axis, normal));
        const math::Vec3 bitangent = math::cross(normal, tangent);
        const float uvScale = radius > 0.0f ? k_discRadius / radius : 1.0f;
        const auto capVertex = [&](const math::Vec3& position, render::SkinnedPbrVertex vertex)
        {
            const math::Vec3 offset = position - centroid;
            vertex.position[0] = position.x;
            vertex.position[1] = position.y;
            vertex.position[2] = position.z;
            vertex.normal[0] = normal.x;
            vertex.normal[1] = normal.y;
            vertex.normal[2] = normal.z;
            vertex.uv[0] = k_discCentreU + std::clamp(math::dot(offset, tangent) * uvScale, -k_discRadius, k_discRadius);
            vertex.uv[1] = k_discCentreV + std::clamp(math::dot(offset, bitangent) * uvScale, -k_discRadius, k_discRadius);
            vertex.tangent[0] = tangent.x;
            vertex.tangent[1] = tangent.y;
            vertex.tangent[2] = tangent.z;
            vertex.tangent[3] = 1.0f;
            math::expand(geometry.bounds, position);
            geometry.vertices.push_back(vertex);
        };

        const auto base = static_cast<uint32_t>(geometry.vertices.size());

        for (const uint32_t welded : loop.vertices)
        {
            const render::SkinnedPbrVertex& vertex = sourceVertex(model, mesh.sources[welded]);
            capVertex(positionOf(vertex), vertex);
        }

        render::SkinnedPbrVertex centre{};
        const auto weights = strongest(centreWeights, side.fallbackJoint);

        for (std::size_t influence = 0; influence < 4; ++influence)
        {
            centre.joints[influence] = static_cast<float>(weights[influence].first);
            centre.weights[influence] = weights[influence].second;
        }

        const float recess = k_recessShare * radius;
        capVertex(centroid - normal * recess, centre);
        const auto centreIndex = static_cast<uint32_t>(base + count);
        const std::size_t edges = loop.isClosed ? count : count - 1;

        for (std::size_t index = 0; index < edges; ++index)
        {
            const uint32_t from = base + static_cast<uint32_t>(index);
            const uint32_t to = base + static_cast<uint32_t>((index + 1) % count);
            const std::array<uint32_t, 3> triangle = closesEdgeSide ? std::array<uint32_t, 3>{to, from, centreIndex}
                                                                    : std::array<uint32_t, 3>{from, to, centreIndex};
            geometry.indices.insert(geometry.indices.end(), triangle.begin(), triangle.end());
        }
    }

    void addWoundStump(const assets::GltfSkinnedModel& model, const WeldedMesh& mesh, const std::vector<Limb>& jointLimbs,
        const std::vector<CapLoop>& loops, const CapSide& side, const math::Vec3& outwards, const WoundTemplate& wound,
        assets::SkinnedGeometry& geometry)
    {
        std::vector<math::Vec3> rim;
        Weights weights;

        for (const CapLoop& loop : loops)
        {
            for (const uint32_t welded : loop.vertices)
            {
                const render::SkinnedPbrVertex& vertex = sourceVertex(model, mesh.sources[welded]);
                rim.push_back(positionOf(vertex));
                addSideWeights(vertex, jointLimbs, side, weights);
            }
        }

        if (rim.empty() || wound.vertices.empty())
        {
            return;
        }

        math::Vec3 centre{0.0f, 0.0f, 0.0f};

        for (const math::Vec3& point : rim)
        {
            centre += point / static_cast<float>(rim.size());
        }

        float radius = 0.0f;

        for (const math::Vec3& point : rim)
        {
            const math::Vec3 offset = point - centre;
            radius += math::length(offset - outwards * math::dot(offset, outwards)) / static_cast<float>(rim.size());
        }

        radius *= k_woundRadiusShare;
        const math::Quat turn = rotationFromUp(outwards);
        const math::Vec3 top = centre + outwards * (k_woundStandShare * radius);
        const auto skin = strongest(weights, side.fallbackJoint);
        const auto base = static_cast<uint32_t>(geometry.vertices.size());

        for (const render::PbrVertex& source : wound.vertices)
        {
            const math::Vec3 local{source.position[0], source.position[1], source.position[2]};
            const math::Vec3 position = top + turn * (local * radius);
            const math::Vec3 normal = turn * math::Vec3{source.normal[0], source.normal[1], source.normal[2]};
            const math::Vec3 tangent = turn * math::Vec3{source.tangent[0], source.tangent[1], source.tangent[2]};
            render::SkinnedPbrVertex vertex{};
            vertex.position[0] = position.x;
            vertex.position[1] = position.y;
            vertex.position[2] = position.z;
            vertex.normal[0] = normal.x;
            vertex.normal[1] = normal.y;
            vertex.normal[2] = normal.z;
            vertex.uv[0] = source.uv[0];
            vertex.uv[1] = source.uv[1];
            vertex.tangent[0] = tangent.x;
            vertex.tangent[1] = tangent.y;
            vertex.tangent[2] = tangent.z;
            vertex.tangent[3] = source.tangent[3];

            for (std::size_t influence = 0; influence < 4; ++influence)
            {
                vertex.joints[influence] = static_cast<float>(skin[influence].first);
                vertex.weights[influence] = skin[influence].second;
            }

            math::expand(geometry.bounds, position);
            geometry.vertices.push_back(vertex);
        }

        for (const uint32_t index : wound.indices)
        {
            geometry.indices.push_back(base + index);
        }
    }

    void addInnerShell(const assets::GltfSkinnedModel& model, const WeldedMesh& mesh,
        const std::vector<uint32_t>& triangles, float depth, assets::SkinnedGeometry& geometry)
    {
        for (const uint32_t index : triangles)
        {
            const Triangle& triangle = mesh.triangles[index];
            const auto base = static_cast<uint32_t>(geometry.vertices.size());

            for (const uint32_t corner : triangle.vertices)
            {
                render::SkinnedPbrVertex vertex = model.parts[triangle.part].geometry.vertices[corner];

                for (std::size_t axis = 0; axis < 3; ++axis)
                {
                    vertex.position[axis] -= vertex.normal[axis] * depth;
                    vertex.normal[axis] = -vertex.normal[axis];
                }

                vertex.tangent[3] = -vertex.tangent[3];
                math::expand(geometry.bounds, positionOf(vertex));
                geometry.vertices.push_back(vertex);
            }

            geometry.indices.insert(geometry.indices.end(), {base, base + 2, base + 1});
        }
    }
} //namespace lockstep::cooker::limbmesh
