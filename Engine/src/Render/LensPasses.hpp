#ifndef LOCKSTEP_RENDER_LENS_PASSES_HPP
#define LOCKSTEP_RENDER_LENS_PASSES_HPP

#include "Lockstep/Core/Result.hpp"

namespace vela::graphics
{
    class RenderGraph;
} //namespace vela::graphics

namespace lockstep::render
{
    struct RendererState;

    // Adds the output pass into the display image (shaders/fxaa.frag): the anti-aliasing filter
    // (FXAA on the tonemapped image, or sharpening of the TAA result), then the camera lens effects
    // (depth of field, motion blur along the scene's motion vectors, chromatic aberration;
    // shaders/lens_effects.glsl), film grain, the screen fade and dithering. Reads the LDR image, the
    // TAA history, the scene depth and the velocity buffer.
    // graph:   the frame graph, already holding tonemapping and the TAA resolve.
    // state:   the renderer state the pass reads every frame. Must outlive the graph.
    // Returns: success, or an error if the shader is missing or the graph rejected the pass.
    [[nodiscard]] Status addLensOutputPass(vela::graphics::RenderGraph& graph, RendererState& state);
} //namespace lockstep::render

#endif //LOCKSTEP_RENDER_LENS_PASSES_HPP
