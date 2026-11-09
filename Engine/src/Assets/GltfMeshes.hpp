#ifndef LOCKSTEP_ASSETS_GLTF_MESHES_HPP
#define LOCKSTEP_ASSETS_GLTF_MESHES_HPP

#include "Lockstep/Core/Bounds.hpp"
#include "Lockstep/Core/Result.hpp"
#include "Lockstep/Render/PbrMaterial.hpp"

#include "ImportWarnings.hpp"

#include <cgltf.h>

#include <cstdint>
#include <vector>

namespace lockstep::assets
{
    // CPU geometry of one glTF primitive, ready for upload.
    // vertices: PBR vertices with normals and tangents (generated when the file has none).
    // indices:  triangle list.
    // bounds:   bounds of the positions.
    struct PrimitiveGeometry
    {
        std::vector<render::PbrVertex> vertices;
        std::vector<uint32_t> indices;
        math::Aabb bounds{};
    };

    // Reads a primitive's positions, normals, TEXCOORD_0, tangents and indices (any component
    // type, sparse accessors included). Triangle strips and fans become triangle lists.
    // primitive: the primitive.
    // warnings:  receives what was ignored or generated (other UV sets, skinning, morph targets).
    // Returns:   the geometry, or an error for primitives that can't be drawn as triangles
    //            (points, lines, Draco compression, no positions, out-of-range indices).
    [[nodiscard]] Result<PrimitiveGeometry> readPrimitive(const cgltf_primitive& primitive, ImportWarnings& warnings);
} //namespace lockstep::assets

#endif //LOCKSTEP_ASSETS_GLTF_MESHES_HPP
