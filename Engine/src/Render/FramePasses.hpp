#ifndef LOCKSTEP_RENDER_FRAME_PASSES_HPP
#define LOCKSTEP_RENDER_FRAME_PASSES_HPP

#include "Lockstep/Core/Result.hpp"

#include <Vela/Graphics/Material.hpp>
#include <Vela/Graphics/Pass.hpp>

#include <memory>
#include <string_view>

namespace vela::core
{
    class Context;
} //namespace vela::core

namespace lockstep::render
{
    struct RendererState;

    // Loads two staged shaders and creates a material from them.
    // context:      the GPU context.
    // vertexFile:   vertex shader .spv file name.
    // fragmentFile: fragment shader .spv file name.
    // layout:       vertex layout (empty for fullscreen triangles).
    // renderState:  depth, culling and blending.
    // Returns:      the material, or an error if a shader is missing or creation failed.
    [[nodiscard]] Result<vela::graphics::Material> createBuiltinMaterial(vela::core::Context& context,
        std::string_view vertexFile, std::string_view fragmentFile, const vela::graphics::VertexLayout& layout,
        const vela::graphics::RenderState& renderState);

    // First pass of the frame. Uploads the frame's lighting block (it must happen inside the graph,
    // after Vela has advanced its frame slot), then renders the procedural sky into the environment
    // atlas: a plain sky tile, prefiltered specular tiles and an irradiance tile. Vela attachments
    // do not persist between frames, so the atlas is rebuilt every frame (it is small).
    class EnvironmentPass final : public vela::graphics::Pass
    {
    public:
        // Loads the environment shader and creates its material.
        // state:   the renderer state whose prepared lighting and light buffer the pass uses every
        //          frame. Must outlive the pass.
        // Returns: the pass, or an error if a shader is missing or the material failed.
        [[nodiscard]] static Result<std::unique_ptr<EnvironmentPass>> create(RendererState& state);

        // Returns: the environment atlas as the only output.
        [[nodiscard]] vela::graphics::PassDescription describe() const override;

        // Uploads the lighting block and draws the atlas (skipped when the sky is off).
        // recorder: Vela's recorder for this pass.
        void record(vela::graphics::PassRecorder& recorder) override;

    private:
        EnvironmentPass(RendererState& state, vela::graphics::Material&& material);

        RendererState* m_state{nullptr};
        vela::graphics::Material m_material;
    };

    // Renders the depth of every shadow caster into its tiles of the fixed-size shadow atlas: the
    // sun's cascades and possibly one spot light. Alpha-masked materials discard in their own
    // shadow material. Every shadow material renders with the hardware depth bias of the shadow
    // settings (see updateShadowMaterials()). The atlas is cleared even when nothing casts, which
    // keeps the graph static.
    class ShadowPass final : public vela::graphics::Pass
    {
    public:
        // Creates the pass. It draws opaque casters with RendererState::shadowMaterial, which must
        // exist before the first frame.
        // state:   the renderer state whose draw lists, prepared lighting and shadow materials the
        //          pass reads every frame. Must outlive the pass.
        // Returns: the pass.
        [[nodiscard]] static Result<std::unique_ptr<ShadowPass>> create(RendererState& state);

        // Returns: the shadow atlas as the only (depth) output.
        [[nodiscard]] vela::graphics::PassDescription describe() const override;

        // Draws each tile's casters, scissored to the tile.
        // recorder: Vela's recorder for this pass.
        void record(vela::graphics::PassRecorder& recorder) override;

    private:
        explicit ShadowPass(const RendererState& state);

        const RendererState* m_state{nullptr};
    };

    // The forward scene pass: clears the HDR targets and depth, draws visible opaque draws front to
    // back, the sky behind them, then alpha-blended draws back to front. Lit materials write direct
    // light to the colour target and environment light to the ambient target (so SSAO can darken
    // only the latter); unlit materials and the sky write colour only.
    class LitScenePass final : public vela::graphics::Pass
    {
    public:
        // Loads the sky shaders and creates the sky material.
        // state:   the renderer state whose draw lists and resources the pass uses every frame.
        //          Must outlive the pass.
        // Returns: the pass, or an error if a shader is missing or the material failed.
        [[nodiscard]] static Result<std::unique_ptr<LitScenePass>> create(RendererState& state);

        // Returns: colour, ambient and depth outputs (cleared; multisampled and resolved into
        //          them when RendererState::sceneSamples > 1); the shadow atlas and environment
        //          atlas as sampled inputs.
        [[nodiscard]] vela::graphics::PassDescription describe() const override;

        // Draws the frame's draw lists and the sky.
        // recorder: Vela's recorder for this pass.
        void record(vela::graphics::PassRecorder& recorder) override;

    private:
        LitScenePass(const RendererState& state, vela::graphics::Material&& skyMaterial);

        const RendererState* m_state{nullptr};
        vela::graphics::Material m_skyMaterial;
    };
} //namespace lockstep::render

#endif //LOCKSTEP_RENDER_FRAME_PASSES_HPP
