#ifndef LOCKSTEP_RENDER_FRAME_PASSES_HPP
#define LOCKSTEP_RENDER_FRAME_PASSES_HPP

#include "Lockstep/Core/Result.hpp"

#include <Vela/Graphics/Material.hpp>
#include <Vela/Graphics/Pass.hpp>

#include <memory>

namespace lockstep::render
{
    struct RendererState;

    // Renders the depth of every lit draw from the frame's shadow-casting light into the
    // fixed-size shadow map. Records nothing when the frame has no shadow caster (the map is still
    // cleared, which keeps the graph static).
    class ShadowPass final : public vela::graphics::Pass
    {
    public:
        // Loads the shadow shaders and creates the depth-only material.
        // state:   the renderer state whose draws and prepared lighting the pass reads every frame.
        //          Must outlive the pass.
        // Returns: the pass, or an error if a shader is missing or the material failed.
        [[nodiscard]] static Result<std::unique_ptr<ShadowPass>> create(RendererState& state);

        // Returns: the shadow map as the only (depth) output.
        [[nodiscard]] vela::graphics::PassDescription describe() const override;

        // Draws the frame's lit draws with the caster's view-projection.
        // recorder: Vela's recorder for this pass.
        void record(vela::graphics::PassRecorder& recorder) override;

    private:
        ShadowPass(const RendererState& state, vela::graphics::Material&& material);

        const RendererState* m_state{nullptr};
        vela::graphics::Material m_material;
    };

    // The forward scene pass: clears the HDR colour and depth targets and draws every submitted
    // draw with its own material. Lit materials get the frame's lighting block (uploaded here)
    // and the shadow map; unlit ones only their model matrix.
    class LitScenePass final : public vela::graphics::Pass
    {
    public:
        // state: the renderer state whose draws, prepared lighting and light buffer the pass uses
        //        every frame. Must outlive the pass.
        explicit LitScenePass(RendererState& state);

        // Returns: HDR colour and depth outputs (cleared), the shadow map as a sampled input.
        [[nodiscard]] vela::graphics::PassDescription describe() const override;

        // Uploads the frame's lighting block and draws the frame's draws.
        // recorder: Vela's recorder for this pass.
        void record(vela::graphics::PassRecorder& recorder) override;

    private:
        RendererState* m_state{nullptr};
    };

    // Turns the HDR scene colour into the displayable image: exposure, a tonemapping curve and
    // dithering, as one fullscreen triangle.
    class TonemapPass final : public vela::graphics::Pass
    {
    public:
        // Loads the fullscreen and tonemap shaders and creates the material.
        // state:   the renderer state whose tonemap settings the pass reads every frame. Must
        //          outlive the pass.
        // Returns: the pass, or an error if a shader is missing or the material failed.
        [[nodiscard]] static Result<std::unique_ptr<TonemapPass>> create(RendererState& state);

        // Returns: the scene colour as sampled input, the display image as the only output.
        [[nodiscard]] vela::graphics::PassDescription describe() const override;

        // Draws the fullscreen triangle.
        // recorder: Vela's recorder for this pass.
        void record(vela::graphics::PassRecorder& recorder) override;

    private:
        TonemapPass(const RendererState& state, vela::graphics::Material&& material);

        const RendererState* m_state{nullptr};
        vela::graphics::Material m_material;
    };
} //namespace lockstep::render

#endif //LOCKSTEP_RENDER_FRAME_PASSES_HPP
