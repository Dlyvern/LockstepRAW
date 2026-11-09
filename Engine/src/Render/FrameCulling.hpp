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
    // draws front to back, visible blended draws back to front, and per shadow atlas tile the
    // draws casting into it. Only draws on the camera's render layers take part. Respects
    // RenderSettings::culling.
    // state:          the renderer state, with draws, prepared lighting and camera set.
    // viewProjection: the camera's world-to-clip matrix.
    void buildDrawLists(RendererState& state, const math::Mat4& viewProjection);
} //namespace lockstep::render

#endif //LOCKSTEP_RENDER_FRAME_CULLING_HPP
