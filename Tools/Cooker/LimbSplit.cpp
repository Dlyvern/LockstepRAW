#include "LimbSplit.hpp"

#include "LimbMesh.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <map>
#include <optional>
#include <utility>

namespace lockstep::cooker
{
    namespace
    {
        using anim::Limb;
        using anim::LimbMask;
        using limbmesh::CapLoop;
        using limbmesh::CapSide;
        using limbmesh::Triangle;
        using limbmesh::VertexRef;
        using limbmesh::WeldedMesh;
        using limbmesh::positionOf;
        using limbmesh::sourceVertex;

        constexpr std::array<std::pair<Limb, std::string_view>, anim::k_limbCount - 1> k_limbRoots{{
            {Limb::Head, "Neck"},
            {Limb::UpperArmLeft, "LeftArm"},
            {Limb::ForearmLeft, "LeftForeArm"},
            {Limb::UpperArmRight, "RightArm"},
            {Limb::ForearmRight, "RightForeArm"},
            {Limb::ThighLeft, "LeftUpLeg"},
            {Limb::ShinLeft, "LeftLeg"},
            {Limb::ThighRight, "RightUpLeg"},
            {Limb::ShinRight, "RightLeg"}}};
        constexpr std::size_t k_hullDirections{48};
        constexpr double k_weldShare{1.0e-6};
        constexpr uint32_t k_unmapped{std::numeric_limits<uint32_t>::max()};
        constexpr float k_openLoopReach{0.06f};
        constexpr float k_stumpShellShare{0.8f};
        constexpr float k_shellDepthShare{0.025f};
        constexpr float k_unlimitedReach{std::numeric_limits<float>::max()};


        math::Vec3 bindPosition(const anim::Skeleton& skeleton, uint32_t joint)
        {
            return math::transformPoint(math::inverse(skeleton.joints[joint].inverseBind), math::Vec3{0.0f, 0.0f, 0.0f});
        }

        math::Vec3 boneDirection(const anim::Skeleton& skeleton, uint32_t root, const math::Vec3& fallback)
        {
            for (std::size_t index = 0; index < skeleton.joints.size(); ++index)
            {
                if (skeleton.joints[index].parent != root)
                {
                    continue;
                }

                const math::Vec3 along = bindPosition(skeleton, static_cast<uint32_t>(index)) - bindPosition(skeleton, root);

                if (math::lengthSquared(along) > 1.0e-10f)
                {
                    return math::normalize(along);
                }
            }

            return fallback;
        }

        std::string_view baseName(std::string_view name)
        {
            const std::size_t colon = name.rfind(':');
            return colon == std::string_view::npos ? name : name.substr(colon + 1);
        }

        Result<std::vector<uint32_t>> findRootJoints(const anim::Skeleton& skeleton)
        {
            std::vector<uint32_t> roots;

            for (const auto& [limb, name] : k_limbRoots)
            {
                uint32_t found = anim::k_noJoint;

                for (std::size_t index = 0; index < skeleton.joints.size() && found == anim::k_noJoint; ++index)
                {
                    if (baseName(skeleton.joints[index].name) == name)
                    {
                        found = static_cast<uint32_t>(index);
                    }
                }

                if (found == anim::k_noJoint)
                {
                    return Error{ErrorCode::InvalidArgument, "no '" + std::string(name) + "' joint for the "
                        + anim::toString(limb)};
                }

                roots.push_back(found);
            }

            return roots;
        }

        std::vector<Limb> classifyJoints(const anim::Skeleton& skeleton, const std::vector<uint32_t>& roots)
        {
            std::vector<Limb> limbs(skeleton.joints.size(), Limb::Torso);

            for (std::size_t index = 0; index < skeleton.joints.size(); ++index)
            {
                for (uint32_t walk = static_cast<uint32_t>(index); walk != anim::k_noJoint;
                     walk = skeleton.joints[walk].parent)
                {
                    const auto root = std::find(roots.begin(), roots.end(), walk);

                    if (root != roots.end())
                    {
                        limbs[index] = k_limbRoots[static_cast<std::size_t>(root - roots.begin())].first;
                        break;
                    }
                }
            }

            return limbs;
        }

        Limb triangleLimb(const assets::SkinnedGeometry& geometry, std::span<const uint32_t, 3> vertices,
            const std::vector<Limb>& jointLimbs)
        {
            std::array<float, anim::k_limbCount> scores{};

            for (const uint32_t index : vertices)
            {
                const render::SkinnedPbrVertex& vertex = geometry.vertices[index];

                for (std::size_t influence = 0; influence < 4; ++influence)
                {
                    const auto joint = static_cast<std::size_t>(vertex.joints[influence]);

                    if (joint < jointLimbs.size())
                    {
                        scores[static_cast<std::size_t>(jointLimbs[joint])] += vertex.weights[influence];
                    }
                }
            }

            return static_cast<Limb>(std::max_element(scores.begin(), scores.end()) - scores.begin());
        }

        WeldedMesh weld(const assets::GltfSkinnedModel& model, const std::vector<Limb>& jointLimbs)
        {
            math::Aabb bounds{};

            for (const assets::SkinnedPart& part : model.parts)
            {
                math::expand(bounds, part.geometry.bounds);
            }

            const double cell = std::max(static_cast<double>(math::length(math::size(bounds))) * k_weldShare, 1.0e-9);
            std::map<std::array<int64_t, 3>, uint32_t> cells;
            WeldedMesh mesh{};

            for (std::size_t part = 0; part < model.parts.size(); ++part)
            {
                const assets::SkinnedGeometry& geometry = model.parts[part].geometry;
                std::vector<uint32_t> welded(geometry.vertices.size());

                for (std::size_t vertex = 0; vertex < geometry.vertices.size(); ++vertex)
                {
                    const float* position = geometry.vertices[vertex].position;
                    const std::array<int64_t, 3> key{std::llround(position[0] / cell), std::llround(position[1] / cell),
                        std::llround(position[2] / cell)};
                    const auto [found, isNew] = cells.emplace(key, static_cast<uint32_t>(mesh.sources.size()));

                    if (isNew)
                    {
                        mesh.sources.push_back(VertexRef{static_cast<uint32_t>(part), static_cast<uint32_t>(vertex)});
                    }

                    welded[vertex] = found->second;
                }

                for (std::size_t first = 0; first + 2 < geometry.indices.size(); first += 3)
                {
                    Triangle triangle{.part = static_cast<uint32_t>(part)};

                    for (std::size_t corner = 0; corner < 3; ++corner)
                    {
                        triangle.vertices[corner] = geometry.indices[first + corner];
                        triangle.welded[corner] = welded[triangle.vertices[corner]];
                    }

                    triangle.limb = triangleLimb(geometry, triangle.vertices, jointLimbs);
                    const auto index = static_cast<uint32_t>(mesh.triangles.size());

                    for (std::size_t corner = 0; corner < 3; ++corner)
                    {
                        const uint32_t from = triangle.welded[corner];
                        const uint32_t to = triangle.welded[(corner + 1) % 3];

                        if (from != to)
                        {
                            mesh.edges[{from, to}].push_back(index);
                        }
                    }

                    mesh.triangles.push_back(triangle);
                }
            }

            return mesh;
        }

        std::vector<LimbSplitPart> regionParts(const assets::GltfSkinnedModel& model, const WeldedMesh& mesh,
            std::span<const uint32_t> materials, std::span<const std::string> names)
        {
            std::vector<LimbSplitPart> parts;

            for (std::size_t part = 0; part < model.parts.size(); ++part)
            {
                const assets::SkinnedGeometry& source = model.parts[part].geometry;

                for (std::size_t limb = 0; limb < anim::k_limbCount; ++limb)
                {
                    LimbSplitPart split{.geometry = {}, .material = materials[part], .materialName = names[part],
                        .limb = static_cast<Limb>(limb), .role = anim::LimbPartRole::Region};
                    std::vector<uint32_t> remap(source.vertices.size(), k_unmapped);

                    for (const Triangle& triangle : mesh.triangles)
                    {
                        if (triangle.part != part || triangle.limb != split.limb)
                        {
                            continue;
                        }

                        for (const uint32_t vertex : triangle.vertices)
                        {
                            if (remap[vertex] == k_unmapped)
                            {
                                remap[vertex] = static_cast<uint32_t>(split.geometry.vertices.size());
                                split.geometry.vertices.push_back(source.vertices[vertex]);
                                math::expand(split.geometry.bounds, positionOf(source.vertices[vertex]));
                            }

                            split.geometry.indices.push_back(remap[vertex]);
                        }
                    }

                    if (!split.geometry.indices.empty())
                    {
                        parts.push_back(std::move(split));
                    }
                }
            }

            return parts;
        }

        std::vector<anim::LimbHullPoint> limbHull(const assets::GltfSkinnedModel& model, const WeldedMesh& mesh, Limb limb)
        {
            std::vector<uint32_t> region;

            for (const Triangle& triangle : mesh.triangles)
            {
                if (triangle.limb == limb)
                {
                    region.insert(region.end(), triangle.welded.begin(), triangle.welded.end());
                }
            }

            std::sort(region.begin(), region.end());
            region.erase(std::unique(region.begin(), region.end()), region.end());
            std::vector<uint32_t> extremes;
            const float golden = math::k_pi * (3.0f - std::sqrt(5.0f));

            for (std::size_t index = 0; index < k_hullDirections && !region.empty(); ++index)
            {
                const float y = 1.0f - 2.0f * (static_cast<float>(index) + 0.5f) / static_cast<float>(k_hullDirections);
                const float ring = std::sqrt(std::max(0.0f, 1.0f - y * y));
                const float angle = golden * static_cast<float>(index);
                const math::Vec3 direction{ring * std::cos(angle), y, ring * std::sin(angle)};
                const auto farthest = std::max_element(region.begin(), region.end(), [&](uint32_t a, uint32_t b)
                {
                    return math::dot(positionOf(sourceVertex(model, mesh.sources[a])), direction)
                        < math::dot(positionOf(sourceVertex(model, mesh.sources[b])), direction);
                });
                extremes.push_back(*farthest);
            }

            std::sort(extremes.begin(), extremes.end());
            extremes.erase(std::unique(extremes.begin(), extremes.end()), extremes.end());
            std::vector<anim::LimbHullPoint> hull;

            for (const uint32_t welded : extremes)
            {
                const render::SkinnedPbrVertex& vertex = sourceVertex(model, mesh.sources[welded]);
                anim::LimbHullPoint point{.position = positionOf(vertex)};

                for (std::size_t influence = 0; influence < 4; ++influence)
                {
                    point.joints[influence] = static_cast<uint8_t>(vertex.joints[influence]);
                    point.weights[influence] = vertex.weights[influence];
                }

                hull.push_back(point);
            }

            return hull;
        }

        struct ShellSide
        {
            LimbMask inside{0};
            bool isInsideSide{true};
            math::Vec3 joint{0.0f, 0.0f, 0.0f};
            float reach{0.0f};
        };

        std::vector<uint32_t> shellTriangles(const assets::GltfSkinnedModel& model, const WeldedMesh& mesh,
            const CapLoop& loop, const ShellSide& side)
        {
            std::vector<bool> isShellPart(model.parts.size(), false);

            for (const uint32_t welded : loop.vertices)
            {
                isShellPart[mesh.sources[welded].part] = true;
            }

            std::vector<uint32_t> triangles;

            for (std::size_t index = 0; index < mesh.triangles.size(); ++index)
            {
                const Triangle& triangle = mesh.triangles[index];
                const bool isInside = (side.inside & anim::limbBit(triangle.limb)) != 0;

                if (isInside != side.isInsideSide || !isShellPart[triangle.part])
                {
                    continue;
                }

                math::Vec3 centre{0.0f, 0.0f, 0.0f};

                for (const uint32_t corner : triangle.vertices)
                {
                    centre += positionOf(model.parts[triangle.part].geometry.vertices[corner]) / 3.0f;
                }

                if (math::length(centre - side.joint) <= side.reach)
                {
                    triangles.push_back(static_cast<uint32_t>(index));
                }
            }

            return triangles;
        }
    } //namespace

    Result<LimbSplit> splitLimbs(const assets::GltfSkinnedModel& model, std::span<const uint32_t> materials,
        std::span<const std::string> names, uint32_t capMaterial, const WoundTemplate* wound)
    {
        auto roots = findRootJoints(model.skeleton);

        if (!roots)
        {
            return roots.error();
        }

        const std::vector<Limb> jointLimbs = classifyJoints(model.skeleton, roots.value());
        const WeldedMesh mesh = weld(model, jointLimbs);
        LimbSplit split{.parts = regionParts(model, mesh, materials, names), .limbs = {}, .capLoops = 0};
        const float reach = k_openLoopReach * math::length(math::size(model.bindBounds));

        for (std::size_t root = 0; root < k_limbRoots.size(); ++root)
        {
            const Limb limb = k_limbRoots[root].first;
            const uint32_t rootJoint = roots.value()[root];
            const LimbMask inside = anim::limbSubtree(limb);
            const auto outside = static_cast<LimbMask>(~inside);
            const uint32_t parentJoint = model.skeleton.joints[rootJoint].parent;
            const CapSide stumpSide{.joints = outside, .isInside = false,
                .fallbackJoint = parentJoint != anim::k_noJoint ? parentJoint : rootJoint};
            const CapSide pieceSide{.joints = inside, .isInside = true, .fallbackJoint = rootJoint};
            std::vector<CapLoop> loops = limbmesh::cutLoops(mesh, inside);
            std::vector<CapLoop> stumpLoops = loops;
            std::vector<CapLoop> pieceLoops = std::move(loops);

            LimbSplitPart stump{.geometry = {}, .material = capMaterial, .materialName = k_woundMaterialName,
                .limb = limb, .role = anim::LimbPartRole::StumpCap};
            LimbSplitPart piece{.geometry = {}, .material = capMaterial, .materialName = k_woundMaterialName,
                .limb = limb, .role = anim::LimbPartRole::PieceCap};

            if (stumpLoops.empty())
            {
                const math::Vec3 joint = math::transformPoint(math::inverse(model.skeleton.joints[rootJoint].inverseBind),
                    math::Vec3{0.0f, 0.0f, 0.0f});

                for (const bool isInsideSide : {false, true})
                {
                    auto open = limbmesh::nearestOpenLoop(model, mesh, inside, isInsideSide, joint, reach);

                    if (!open)
                    {
                        continue;
                    }

                    const std::vector<uint32_t> shell = shellTriangles(model, mesh, *open,
                        ShellSide{.inside = inside, .isInsideSide = isInsideSide, .joint = joint,
                            .reach = isInsideSide ? k_unlimitedReach : k_stumpShellShare * reach});
                    limbmesh::addInnerShell(model, mesh, shell, k_shellDepthShare * reach,
                        isInsideSide ? piece.geometry : stump.geometry);
                    (isInsideSide ? pieceLoops : stumpLoops).push_back(std::move(*open));
                }
            }

            for (const CapLoop& loop : stumpLoops)
            {
                limbmesh::addCapLoop(model, mesh, jointLimbs, loop, stumpSide, stump.geometry);
            }

            for (const CapLoop& loop : pieceLoops)
            {
                limbmesh::addCapLoop(model, mesh, jointLimbs, loop, pieceSide, piece.geometry);
            }

            if (wound != nullptr)
            {
                const math::Vec3 bone = boneDirection(model.skeleton, rootJoint, math::Vec3{0.0f, 1.0f, 0.0f});
                limbmesh::addWoundStump(model, mesh, jointLimbs, stumpLoops, stumpSide, bone, *wound, stump.geometry);
                limbmesh::addWoundStump(model, mesh, jointLimbs, pieceLoops, pieceSide, -bone, *wound, piece.geometry);
            }

            split.capLoops += static_cast<uint32_t>(std::max(stumpLoops.size(), pieceLoops.size()));

            for (LimbSplitPart* cap : {&stump, &piece})
            {
                if (!cap->geometry.indices.empty())
                {
                    split.parts.push_back(std::move(*cap));
                }
            }

            split.limbs.push_back(anim::LimbInfo{.limb = limb, .rootJoint = rootJoint, .hull = limbHull(model, mesh, limb)});
        }

        return split;
    }
} //namespace lockstep::cooker
