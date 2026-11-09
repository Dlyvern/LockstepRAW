#ifndef LOCKSTEP_RENDER_FRAME_CULLING_HPP
#define LOCKSTEP_RENDER_FRAME_CULLING_HPP

#include "Lockstep/Core/Bounds.hpp"
#include "Lockstep/Core/Math.hpp"

namespace lockstep::render
{
    struct RendererState;

    // The six planes of a view volume, xyz the inward normal and w the offset (a point p is inside
    // a plane when dot(xyz, p) + w >= 0).
    struct Frustum
    {
        math::Vec4 planes[6];
    };

    // Extracts the planes of a Vulkan-style (depth 0..1) view-projection matrix.
    // viewProjection: world to clip space.
    // Returns:        the frustum.
    [[nodiscard]] Frustum frustumFromMatrix(const math::Mat4& viewProjection);

    // frustum: a frustum.
    // box:     a world-space box.
    // Returns: false only if the box is certainly outside the frustum.
    [[nodiscard]] bool intersects(const Frustum& frustum, const math::Aabb& box);

    // Fills the state's draw lists from its submitted draws and prepared lighting: visible opaque
    // draws front to back, visible blended draws back to front, both split into world and
    // viewmodel draws (k_viewModelLayer), and per shadow atlas tile the world draws casting into
    // it. World draws take part only when on the camera's render layers; viewmodel draws always
    // do and never cast shadows. Respects RenderSettings::culling.
    // state:                   the renderer state, with draws, prepared lighting and camera set.
    // viewProjection:          the camera's world-to-clip matrix.
    // viewModelViewProjection: the viewmodel's world-to-clip matrix.
    void buildDrawLists(RendererState& state, const math::Mat4& viewProjection,
        const math::Mat4& viewModelViewProjection);
} //namespace lockstep::render

#endif //LOCKSTEP_RENDER_FRAME_CULLING_HPP
