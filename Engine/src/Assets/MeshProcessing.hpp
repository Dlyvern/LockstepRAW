#ifndef LOCKSTEP_ASSETS_MESH_PROCESSING_HPP
#define LOCKSTEP_ASSETS_MESH_PROCESSING_HPP

#include "Lockstep/Core/Math.hpp"
#include "Lockstep/Render/PbrMaterial.hpp"

#include <cstdint>
#include <span>
#include <vector>

namespace lockstep::assets
{
    // Gives every triangle its own three vertices with the triangle's face normal, as glTF asks
    // for when a primitive has no normals.
    // vertices: the vertices; replaced by the unshared ones.
    // indices:  triangle list; replaced by 0, 1, 2, ...
    void generateFlatNormals(std::vector<render::PbrVertex>& vertices, std::vector<uint32_t>& indices);

    // Fills each vertex's tangent from its triangles' UV directions, accumulated per vertex and
    // orthogonalised against the normal. The handedness follows glTF: the bitangent
    // cross(normal, tangent) * w points along decreasing v (up in the image). Vertices without
    // usable UVs get any tangent perpendicular to the normal.
    // vertices: vertices with positions, normals and uvs.
    // indices:  triangle list into `vertices`.
    void generateTangents(std::span<render::PbrVertex> vertices, std::span<const uint32_t> indices);

    // positions: vertex positions.
    // indices:   triangle list into `positions`.
    // Returns:   the triangles of `indices` that have three distinct corners and a non-zero area.
    [[nodiscard]] std::vector<uint32_t> nonDegenerateTriangles(std::span<const math::Vec3> positions,
        std::span<const uint32_t> indices);
} //namespace lockstep::assets

#endif //LOCKSTEP_ASSETS_MESH_PROCESSING_HPP
