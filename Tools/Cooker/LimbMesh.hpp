#ifndef LOCKSTEP_COOKER_LIMB_MESH_HPP
#define LOCKSTEP_COOKER_LIMB_MESH_HPP

#include <Lockstep/Anim/Limbs.hpp>

#include "Assets/GltfSkins.hpp"
#include "LimbSplit.hpp"

#include <array>
#include <cstdint>
#include <map>
#include <optional>
#include <utility>
#include <vector>

// The welded view of a skinned model that splitLimbs() works on, and the wound cap builder.
namespace lockstep::cooker::limbmesh
{
    // VertexRef:  a vertex of the source model (its part and its index there).
    // Triangle:   a source triangle: its part, source and welded vertex indices, and its limb.
    // WeldedMesh: every triangle with vertices welded by position across parts; `sources` gives
    //             each welded vertex's first source vertex, `edges` maps a directed welded edge
    //             to the triangles that have it.
    // CapLoop:    a boundary loop of welded vertices; isFromInside tells whether its edges come
    //             from the triangles inside the cut.
    struct VertexRef
    {
        uint32_t part{0};
        uint32_t vertex{0};
    };

    struct Triangle
    {
        uint32_t part{0};
        std::array<uint32_t, 3> vertices{};
        std::array<uint32_t, 3> welded{};
        anim::Limb limb{anim::Limb::Torso};
    };

    struct WeldedMesh
    {
        std::vector<VertexRef> sources;
        std::vector<Triangle> triangles;
        std::map<std::pair<uint32_t, uint32_t>, std::vector<uint32_t>> edges;
    };

    struct CapLoop
    {
        std::vector<uint32_t> vertices;
        bool isClosed{false};
        bool isFromInside{true};
    };

    // Which side of a cut a cap closes.
    // joints:        limbs whose joints may move the cap's centre.
    // isInside:      it closes the limb side (the piece), else the body side (the stump).
    // fallbackJoint: moves the centre when no joint of `joints` touches the loop.
    struct CapSide
    {
        anim::LimbMask joints{0};
        bool isInside{true};
        uint32_t fallbackJoint{0};
    };

    // model:     the model.
    // reference: one of its vertices.
    // Returns:   the vertex.
    [[nodiscard]] inline const render::SkinnedPbrVertex& sourceVertex(const assets::GltfSkinnedModel& model,
        VertexRef reference)
    {
        return model.parts[reference.part].geometry.vertices[reference.vertex];
    }

    // vertex:  a skinned vertex.
    // Returns: its bind-pose position.
    [[nodiscard]] inline math::Vec3 positionOf(const render::SkinnedPbrVertex& vertex)
    {
        return math::Vec3{vertex.position[0], vertex.position[1], vertex.position[2]};
    }

    // Finds the loops where the triangles of a set of limbs meet the rest of the body.
    // mesh:    the welded model.
    // inside:  the limbs on the inside of the cut.
    // Returns: the loops, their edges taken from inside triangles.
    [[nodiscard]] std::vector<CapLoop> cutLoops(const WeldedMesh& mesh, anim::LimbMask inside);

    // Finds the open mesh border nearest a joint on one side of a cut (for limbs whose mesh is
    // not joined to the body).
    // model, mesh:  the model and its welded view.
    // inside:       the limbs on the inside of the cut.
    // isInsideSide: look at the inside triangles, else at the others.
    // joint:        the cut's joint in bind pose.
    // reach:        farthest a loop's centre may be from the joint.
    // Returns:      the nearest loop, or nothing within reach.
    [[nodiscard]] std::optional<CapLoop> nearestOpenLoop(const assets::GltfSkinnedModel& model, const WeldedMesh& mesh,
        anim::LimbMask inside, bool isInsideSide, const math::Vec3& joint, float reach);

    // Closes one loop with a slightly recessed fan of triangles facing out of the side it closes,
    // appended to a cap's geometry. The rim keeps the loop vertices' skinning; the centre takes
    // the side's joints only.
    // model, mesh: the model and its welded view.
    // jointLimbs:  per joint, its limb.
    // loop:        the loop.
    // side:        which side it closes.
    // geometry:    receives the cap triangles.
    void addCapLoop(const assets::GltfSkinnedModel& model, const WeldedMesh& mesh, const std::vector<anim::Limb>& jointLimbs,
        const CapLoop& loop, const CapSide& side, assets::SkinnedGeometry& geometry);

    // Places the wound stump template across a cut (every loop of one side together): centred on
    // the loops' vertices, its axis along `outwards`, its wound face a little out of the cut,
    // scaled to the mean distance of the loop vertices from the axis, skinned like a cap centre
    // (the side's joints only).
    // model, mesh: the model and its welded view.
    // jointLimbs:  per joint, its limb.
    // loops:       the side's loops.
    // side:        which side it closes.
    // outwards:    unit direction out of the side it closes, along the bone.
    // wound:       the template.
    // geometry:    receives the stump.
    void addWoundStump(const assets::GltfSkinnedModel& model, const WeldedMesh& mesh,
        const std::vector<anim::Limb>& jointLimbs, const std::vector<CapLoop>& loops, const CapSide& side,
        const math::Vec3& outwards, const WoundTemplate& wound, assets::SkinnedGeometry& geometry);

    // Adds an inner shell of wound-coloured triangles behind a set of source triangles: each one
    // copied a little inwards along its vertex normals, facing inwards. Seen from outside it is
    // back-facing and culled; through an opening (eye holes, a severed neck) it hides the hollow
    // inside of the mesh.
    // model, mesh: the model and its welded view.
    // triangles:   indices into mesh.triangles.
    // depth:       how far inwards, mesh units.
    // geometry:    receives the shell.
    void addInnerShell(const assets::GltfSkinnedModel& model, const WeldedMesh& mesh,
        const std::vector<uint32_t>& triangles, float depth, assets::SkinnedGeometry& geometry);
} //namespace lockstep::cooker::limbmesh

#endif //LOCKSTEP_COOKER_LIMB_MESH_HPP
