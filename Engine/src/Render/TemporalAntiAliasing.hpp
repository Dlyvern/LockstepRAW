#ifndef LOCKSTEP_RENDER_TEMPORAL_ANTI_ALIASING_HPP
#define LOCKSTEP_RENDER_TEMPORAL_ANTI_ALIASING_HPP

#include "Lockstep/Core/Math.hpp"
#include "Lockstep/Core/Result.hpp"

#include <cstdint>
#include <unordered_map>

namespace vela::graphics
{
    class RenderGraph;
} //namespace vela::graphics

namespace lockstep::render
{
    struct RendererState;

    // What TAA carries from one frame to the next.
    // previousModels:  model matrices of last frame's draws, keyed by drawMotionKey().
    // currentModels:   the same for this frame, swapped into previousModels at the end of it.
    // occurrences:     scratch count of draws per mesh and material pair this frame.
    // viewProjection:  this frame's camera matrix without jitter.
    // previousViewProjection: last frame's viewProjection.
    // jitter:          this frame's projection offset in pixels, each axis in [-0.5, 0.5].
    // sampleIndex:     position in the jitter sequence.
    // hasPrevious:     a frame was rendered before this one (the previous matrices are real).
    struct TemporalState
    {
        std::unordered_map<uint64_t, math::Mat4> previousModels;
        std::unordered_map<uint64_t, math::Mat4> currentModels;
        std::unordered_map<uint64_t, uint32_t> occurrences;
        math::Mat4 viewProjection{1.0f};
        math::Mat4 previousViewProjection{1.0f};
        math::Vec2 jitter{0.0f, 0.0f};
        uint32_t sampleIndex{0};
        bool hasPrevious{false};
    };

    // Starts a frame's temporal state: picks the jitter of this frame (none unless TAA is on and
    // jittering), stores the unjittered camera matrix and returns the projection to render with.
    // state:      the renderer state; its projection must be the unjittered one, its view set.
    // width:      render target width in pixels.
    // height:     render target height in pixels.
    // Returns:    the projection with this frame's sub-pixel jitter applied.
    [[nodiscard]] math::Mat4 beginTemporalFrame(RendererState& state, uint32_t width, uint32_t height);

    // Gives every submitted draw the model matrix it had last frame. Draws are matched by mesh,
    // material and their submission order among draws of that pair; unmatched draws are treated
    // as static (previous model = model).
    // state: the renderer state with this frame's draws.
    void assignPreviousModels(RendererState& state);

    // Ends a frame's temporal state: this frame's models and camera become the previous ones.
    // state: the renderer state.
    void endTemporalFrame(RendererState& state);

    // Adds the TAA resolve pass: reads the tonemapped image, motion vectors, depth and its own
    // history (last frame's output), and writes the accumulated image into the history
    // attachment, which the output pass then sharpens. Draws only in AntiAliasingMode::Taa;
    // otherwise it clears the history, which marks it invalid for the next TAA frame.
    // graph: the frame graph.
    // state: the renderer state the pass reads every frame. Must outlive the graph.
    // Returns: success, or an error if the shader is missing or the graph rejected the pass.
    [[nodiscard]] Status addTemporalPass(vela::graphics::RenderGraph& graph, RendererState& state);
} //namespace lockstep::render

#endif //LOCKSTEP_RENDER_TEMPORAL_ANTI_ALIASING_HPP
