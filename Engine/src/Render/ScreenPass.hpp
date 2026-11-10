#ifndef LOCKSTEP_RENDER_SCREEN_PASS_HPP
#define LOCKSTEP_RENDER_SCREEN_PASS_HPP

#include "Lockstep/Core/Result.hpp"

#include <Vela/Graphics/Material.hpp>
#include <Vela/Graphics/Pass.hpp>
#include <Vela/Graphics/RenderTypes.hpp>

#include <functional>
#include <memory>
#include <string_view>
#include <vector>

namespace vela::graphics
{
    class RenderGraph;
} //namespace vela::graphics

namespace lockstep::render
{
    struct RendererState;

    // A pass that draws one fullscreen triangle into one or more colour attachments (same size),
    // reading earlier attachments through set-2 slots 0.. in input order. Unlike FullscreenPass it
    // writes several targets with one material and can follow a renderer-owned resolution scale.
    class ScreenPass final : public vela::graphics::Pass
    {
    public:
        // Records the pass's extra bindings and constants and returns whether to draw this frame.
        // When it returns false cleared targets keep their clear value and loaded ones are kept.
        using Recorder = std::function<bool(vela::graphics::PassRecorder& recorder)>;

        // outputs:  the colour attachments it writes, in fragment output order.
        // inputs:   the attachments it samples, in slot order.
        // material: fullscreen material (fullscreen.vert and a fragment shader).
        // scale:    if not null, every output's scale is read from here on each describe().
        // recorder: binds constants and decides whether to draw.
        ScreenPass(std::vector<vela::graphics::AttachmentSlot> outputs, std::vector<vela::graphics::PassInput> inputs,
            vela::graphics::Material&& material, const float* scale, Recorder recorder);

        // Returns: the outputs (scaled) and the sampled inputs.
        [[nodiscard]] vela::graphics::PassDescription describe() const override;

        // Binds the inputs to slots 0.., runs the recorder and draws the triangle if it says so.
        // recorder: Vela's recorder for this pass.
        void record(vela::graphics::PassRecorder& recorder) override;

    private:
        std::vector<vela::graphics::AttachmentSlot> m_outputs;
        std::vector<vela::graphics::PassInput> m_inputs;
        vela::graphics::Material m_material;
        const float* m_scale{nullptr};
        Recorder m_recorder;
    };

    // How a ScreenPass is set up for addScreenPass().
    // name:           graph pass name.
    // fragmentShader: staged .spv file name.
    // blend:          blend mode of the material (applies to every output).
    // outputs:        written attachments, in fragment output order.
    // inputs:         sampled attachments, in slot order.
    // scale:          optional renderer-owned output scale (see ScreenPass).
    struct ScreenPassDescription
    {
        std::string_view name;
        std::string_view fragmentShader;
        vela::graphics::BlendMode blend{vela::graphics::BlendMode::Opaque};
        std::vector<vela::graphics::AttachmentSlot> outputs;
        std::vector<vela::graphics::PassInput> inputs;
        const float* scale{nullptr};
    };

    // Creates a ScreenPass and adds it to the graph.
    // graph:       the frame graph.
    // state:       the renderer state (GPU context).
    // description: the pass.
    // recorder:    binds constants and decides whether to draw.
    // Returns:     success, or an error if the shader is missing or the graph rejected the pass.
    [[nodiscard]] Status addScreenPass(vela::graphics::RenderGraph& graph, RendererState& state,
        ScreenPassDescription description, ScreenPass::Recorder recorder);

    // Returns: a colour slot cleared to `clear` every frame.
    // name:    attachment name.
    // format:  texel format.
    // scale:   size relative to the window.
    // clear:   clear colour.
    [[nodiscard]] vela::graphics::AttachmentSlot clearedScreenSlot(std::string_view name,
        vela::graphics::TextureFormat format, float scale, const vela::graphics::ClearValue& clear);

    // Returns: a colour slot that is loaded and drawn on top of (full resolution).
    // name:    attachment name.
    // format:  texel format.
    [[nodiscard]] vela::graphics::AttachmentSlot loadedScreenSlot(std::string_view name,
        vela::graphics::TextureFormat format);
} //namespace lockstep::render

#endif //LOCKSTEP_RENDER_SCREEN_PASS_HPP
