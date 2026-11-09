#ifndef LOCKSTEP_RENDER_RENDER_PASS_HPP
#define LOCKSTEP_RENDER_RENDER_PASS_HPP

#include <Vela/Graphics/Pass.hpp>

namespace lockstep::render
{
    // An Engine-side render pass that draws on top of the finished frame, before it is presented.
    // Engine-internal: implementations record with Vela directly, but never see Vela's attachment
    // model. The Renderer decides which attachment the pass draws into (today: the tonemapped
    // display image, loaded and stored, so overlays are not affected by exposure or fog) and runs
    // passes in the order they were added.
    // That order is not declared to Vela: it relies on Vela's graph sort keeping insertion order
    // between passes that write the same attachment (see Vela gaps in CLAUDE.md).
    class RenderPass
    {
    public:
        virtual ~RenderPass() = default;

        // Called by Renderer::render() once per frame, before the render graph records. Runs on
        // every frame, also on frames where record() is skipped (for example a minimized window),
        // so per-frame CPU work that must always happen belongs here.
        virtual void prepareFrame()
        {
        }

        // Records the pass's draw commands. The color target is already bound, the viewport covers
        // it, and it holds the tonemapped frame drawn so far. There is no depth attachment.
        // recorder: Vela's recorder for this pass.
        virtual void record(vela::graphics::PassRecorder& recorder) = 0;
    };
} //namespace lockstep::render

#endif //LOCKSTEP_RENDER_RENDER_PASS_HPP
