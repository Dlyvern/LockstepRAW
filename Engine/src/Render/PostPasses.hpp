#ifndef LOCKSTEP_RENDER_POST_PASSES_HPP
#define LOCKSTEP_RENDER_POST_PASSES_HPP

#include "Lockstep/Core/Result.hpp"

#include <Vela/Graphics/Material.hpp>
#include <Vela/Graphics/Pass.hpp>

#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace vela::graphics
{
    class RenderGraph;
} //namespace vela::graphics

namespace lockstep::render
{
    struct RendererState;

    // A pass that draws one fullscreen triangle into one colour attachment, reading earlier
    // attachments. What it binds and pushes is supplied as a function, so SSAO, bloom, tonemapping
    // and FXAA share this one class.
    class FullscreenPass final : public vela::graphics::Pass
    {
    public:
        // Records the pass's bindings and constants and returns whether to draw this frame. When it
        // returns false the target only keeps its clear value.
        using Recorder = std::function<bool(vela::graphics::PassRecorder& recorder)>;

        // output:   the colour attachment the pass writes (cleared to its clear value).
        // inputs:   the attachments it samples (this frame's, or last frame's for History inputs),
        //           in set-2 slot order.
        // material: fullscreen material (fullscreen.vert and a fragment shader).
        // recorder: binds inputs, pushes constants and decides whether to draw.
        FullscreenPass(vela::graphics::AttachmentSlot output, std::vector<vela::graphics::PassInput> inputs,
            vela::graphics::Material&& material, Recorder recorder);

        // Returns: the output and the sampled inputs.
        [[nodiscard]] vela::graphics::PassDescription describe() const override;

        // Binds the inputs to slots 0.., runs the recorder and draws the triangle if it says so.
        // recorder: Vela's recorder for this pass.
        void record(vela::graphics::PassRecorder& recorder) override;

    private:
        vela::graphics::AttachmentSlot m_output;
        std::vector<vela::graphics::PassInput> m_inputs;
        vela::graphics::Material m_material;
        Recorder m_recorder;
    };

    // Creates a FullscreenPass with fullscreen.vert and a fragment shader and adds it to the graph
    // under its output's name.
    // graph:          the frame graph.
    // state:          the renderer state (GPU context).
    // fragmentShader: staged .spv file name of the fragment shader.
    // output:         the colour attachment the pass writes.
    // inputs:         the attachments it samples, in slot order.
    // recorder:       binds constants and decides whether to draw.
    // Returns:        success, or an error if the shader is missing or the graph rejected the pass.
    [[nodiscard]] Status addFullscreenPass(vela::graphics::RenderGraph& graph, RendererState& state,
        std::string_view fragmentShader, vela::graphics::AttachmentSlot output,
        std::vector<vela::graphics::PassInput> inputs, FullscreenPass::Recorder recorder);

    // Adds the post-processing chain after the lit scene pass: SSAO and its blur, the bloom mip
    // chain, tonemapping into the LDR image, the TAA resolve, and the output pass (FXAA, or TAA
    // sharpening, plus dithering) into the display image.
    // graph: the frame graph, already holding the scene passes.
    // state: the renderer state the passes read every frame. Must outlive the graph.
    // Returns: success, or an error if a shader is missing or the graph rejected a pass.
    [[nodiscard]] Status addPostPasses(vela::graphics::RenderGraph& graph, RendererState& state);
} //namespace lockstep::render

#endif //LOCKSTEP_RENDER_POST_PASSES_HPP
