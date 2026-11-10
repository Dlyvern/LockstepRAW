#ifndef LOCKSTEP_RENDER_VIEW_MODEL_HPP
#define LOCKSTEP_RENDER_VIEW_MODEL_HPP

#include "Lockstep/Render/Views.hpp"

namespace lockstep::render
{
    struct RendererState;

    // Factor the viewmodel's depth is squeezed by: viewmodel pixels have depth buffer values below
    // it, world surfaces (farther than 1.01 times the near plane) above it.
    inline constexpr float k_viewModelDepthScale{0.01f};

    // layers:  a draw's render layers.
    // Returns: true if it is a viewmodel draw (see k_viewModelLayer).
    [[nodiscard]] constexpr bool isViewModelDraw(RenderLayers layers)
    {
        return (layers & k_viewModelLayer) != 0;
    }

    // Computes this frame's viewmodel matrices and writes them, and the camera projection's depth
    // parameters, into the prepared lighting block. The viewmodel projection uses
    // Camera::viewModelFov, a near plane a few millimetres from the eye and the frame's TAA
    // jitter; its depth is squeezed into the first percent of the depth range, in front of every
    // world surface farther than 1.01 times the camera's near plane. The view matrix is the camera's, so viewmodel
    // lighting and shadows use true world positions. Call after beginTemporalFrame() and
    // prepareLighting(); endTemporalFrame() makes the matrices last frame's.
    // state: the renderer state with camera, view, projection (jittered), unjittered projection,
    //        aspect ratio and prepared lighting set for this frame.
    void prepareViewModel(RendererState& state);
} //namespace lockstep::render

#endif //LOCKSTEP_RENDER_VIEW_MODEL_HPP
