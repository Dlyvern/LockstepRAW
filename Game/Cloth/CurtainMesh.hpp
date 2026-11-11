#ifndef LOCKSTEP_GAME_CLOTH_CURTAIN_MESH_HPP
#define LOCKSTEP_GAME_CLOTH_CURTAIN_MESH_HPP

#include "Cloth/ClothPanel.hpp"

#include <Lockstep/Core/Bounds.hpp>
#include <Lockstep/Render/PbrMaterial.hpp>

#include <cstdint>
#include <vector>

namespace game
{
    // A curtain's skinned mesh in panel space.
    // vertices: pleated sheet vertices, each bound to the four particles around it (bilinear).
    // indices:  triangle list, front faces towards +z.
    // bounds:   panel-space bounds with room for the sheet to swing.
    struct CurtainMeshData
    {
        std::vector<lockstep::render::SkinnedPbrVertex> vertices;
        std::vector<uint32_t> indices;
        lockstep::math::Aabb bounds{};
    };

    // Builds the mesh drawn for a cloth panel: a finer grid than the particles with vertical pleats,
    // UVs in metres (for the fabric's micro-detail).
    // panel:   the panel (its size and particle grid).
    // Returns: the mesh data.
    [[nodiscard]] CurtainMeshData buildCurtainMesh(const ClothPanel& panel);
} //namespace game

#endif //LOCKSTEP_GAME_CLOTH_CURTAIN_MESH_HPP
