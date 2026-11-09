#ifndef LOCKSTEP_RENDER_VIEWS_HPP
#define LOCKSTEP_RENDER_VIEWS_HPP

#include "Lockstep/Core/Math.hpp"
#include "Lockstep/Core/Transform.hpp"
#include "Lockstep/Render/Resources.hpp"

#include <cstdint>

namespace lockstep::render
{
    // Render layers: a draw carries a bit mask of layers, and each view (the camera, a planar
    // reflector) draws only what shares a bit with its own mask. Engine gives meaning only to the
    // default layer; Game assigns the other 31 bits (for example "only seen in mirrors").
    using RenderLayers = uint32_t;

    // The layer every draw is on unless Game says otherwise, and the one every view sees by default.
    inline constexpr RenderLayers k_defaultRenderLayer{1u};

    // Every layer.
    inline constexpr RenderLayers k_allRenderLayers{0xFFFFFFFFu};

    // A flat mirror for one frame. The reflector is a mesh instance Game also submits normally; the
    // Renderer finds that draw (same mesh and transform) and composites the reflection on it. The
    // mirror plane is derived from the mesh: its normal is the thinnest axis of the mesh's local
    // bounds, facing the camera, and it lies on the bounds face on the camera's side.
    // mesh:      the mirror's mesh.
    // transform: the mirror's world placement, exactly as submitted.
    // layers:    the layers the reflection shows; Game can show the local pawn (hidden from the
    //            first-person camera) or something else entirely.
    // tint:      linear RGB multiplier on the reflection (1 for a clean mirror).
    struct PlanarReflector
    {
        MeshHandle mesh{};
        math::Transform transform{};
        RenderLayers layers{k_defaultRenderLayer};
        math::Vec3 tint{1.0f, 1.0f, 1.0f};
    };
} //namespace lockstep::render

#endif //LOCKSTEP_RENDER_VIEWS_HPP
