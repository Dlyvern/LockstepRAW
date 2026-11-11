#ifndef LOCKSTEP_RENDER_LENS_FLARE_HPP
#define LOCKSTEP_RENDER_LENS_FLARE_HPP

#include "Lockstep/Core/Result.hpp"

#include <string_view>

namespace vela::graphics
{
    class RenderGraph;
} //namespace vela::graphics

namespace lockstep::render
{
    struct RendererState;

    // The lens flare target: ghosts, halo and sun glare at quarter resolution, in bloom units
    // (shaders/lens_flare.frag); the last bloom upsample adds it.
    inline constexpr std::string_view k_lensFlareAttachment{"lensFlare"};

    // The fragment shader of the last bloom upsample, which adds the lens flare target.
    inline constexpr std::string_view k_bloomUpFlareShader{"bloom_up_flare.frag.spv"};

    // Adds the lens flare pass (LensFlareSettings): reads a bloom down level, the scene depth (no
    // flares from the viewmodel; sun occlusion) and the exposure. Add after the bloom down chain
    // and before the last bloom upsample. Draws nothing (the target stays black) while bloom or
    // both flares and sun glare are off.
    // graph:   the frame graph.
    // state:   the renderer state the pass reads every frame. Must outlive the graph.
    // Returns: success, or an error if the shader is missing or the graph rejected the pass.
    [[nodiscard]] Status addLensFlarePass(vela::graphics::RenderGraph& graph, RendererState& state);
} //namespace lockstep::render

#endif //LOCKSTEP_RENDER_LENS_FLARE_HPP
