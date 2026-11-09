#ifndef LOCKSTEP_RENDER_RENDER_PASS_ADAPTER_HPP
#define LOCKSTEP_RENDER_RENDER_PASS_ADAPTER_HPP

#include "FrameAttachments.hpp"
#include "RenderPass.hpp"

#include <Vela/Graphics/Pass.hpp>

namespace lockstep::render
{
    // The Vela pass the render graph owns for one Engine RenderPass. It declares the tonemapped
    // display attachment as its only output (load and store) and forwards recording to the
    // Engine pass.
    class RenderPassAdapter final : public vela::graphics::Pass
    {
    public:
        // Wraps an Engine pass so Vela's render graph can run it.
        // pass: the Engine pass to forward to. Owned by the RendererState, not by the adapter, and
        //       removed from the graph before it is destroyed.
        explicit RenderPassAdapter(RenderPass& pass);

        // Returns: the display attachment as the only output, loaded and stored.
        [[nodiscard]] vela::graphics::PassDescription describe() const override;

        // Records the Engine pass.
        // recorder: Vela's recorder for this pass.
        void record(vela::graphics::PassRecorder& recorder) override;

    private:
        RenderPass& m_pass;
    };
} //namespace lockstep::render

#endif //LOCKSTEP_RENDER_RENDER_PASS_ADAPTER_HPP
