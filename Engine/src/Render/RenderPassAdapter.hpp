#ifndef LOCKSTEP_RENDER_RENDER_PASS_ADAPTER_HPP
#define LOCKSTEP_RENDER_RENDER_PASS_ADAPTER_HPP

#include "RenderPass.hpp"

#include <Vela/Graphics/Pass.hpp>
#include <Vela/Graphics/RenderTypes.hpp>

#include <string_view>

namespace lockstep::render
{
    // Name and format of the color attachment Vela's built-in scene pass renders into. Engine
    // passes draw on top of it, so their slot must match it exactly.
    inline constexpr std::string_view k_sceneColorAttachment{"color"};
    inline constexpr vela::graphics::TextureFormat k_sceneColorFormat{vela::graphics::TextureFormat::RGBA16Float};

    // The Vela pass the render graph owns for one Engine RenderPass. It declares the scene color
    // attachment as its only output (load and store) and forwards recording to the Engine pass.
    // Vela cannot remove a pass from a graph, so removing an Engine pass detaches it here instead:
    // a detached adapter stays in the graph and records nothing.
    class RenderPassAdapter final : public vela::graphics::Pass
    {
    public:
        // Wraps an Engine pass so Vela's render graph can run it.
        // pass: the Engine pass to forward to. Owned by the RendererState, not by the adapter.
        explicit RenderPassAdapter(RenderPass& pass);

        // Returns: the scene color attachment as the only output, loaded and stored.
        [[nodiscard]] vela::graphics::PassDescription describe() const override;

        // Records the attached Engine pass, or nothing once detached.
        // recorder: Vela's recorder for this pass.
        void record(vela::graphics::PassRecorder& recorder) override;

        // Returns: true if the adapter still forwards to `pass`.
        [[nodiscard]] bool isAttachedTo(const RenderPass& pass) const;

        // Stops forwarding. Call before the Engine pass is destroyed.
        void detach();

    private:
        RenderPass* m_pass{nullptr};
    };
} //namespace lockstep::render

#endif //LOCKSTEP_RENDER_RENDER_PASS_ADAPTER_HPP
