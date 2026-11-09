#ifndef LOCKSTEP_RENDER_SHAPES_HPP
#define LOCKSTEP_RENDER_SHAPES_HPP

#include "Lockstep/Render/Resources.hpp"

namespace lockstep::render::shapes
{
    // Builds a unit cube centered on the origin, with per-face UVs covering the whole texture.
    // Returns: the cube's vertices and triangle-list indices.
    [[nodiscard]] MeshData cube();
} //namespace lockstep::render::shapes

#endif //LOCKSTEP_RENDER_SHAPES_HPP
