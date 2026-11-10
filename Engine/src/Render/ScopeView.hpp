#ifndef LOCKSTEP_RENDER_SCOPE_VIEW_HPP
#define LOCKSTEP_RENDER_SCOPE_VIEW_HPP

#include "Lockstep/Core/Math.hpp"
#include "Lockstep/Core/Result.hpp"

#include <Vela/Graphics/Pass.hpp>

#include <cstdint>
#include <memory>
#include <string_view>
#include <vector>

namespace lockstep::render
{
    struct RendererState;

    // Name of the scope view pass in the frame graph.
    inline constexpr std::string_view k_scopePassName{"scope"};

    // The scope view chosen for one frame.
    // active:         a scope view is rendered this frame.
    // viewProjection: world to clip of the scope view, squeezed into the corner of the target it
    //                is drawn in.
    // texels:         side of that corner in texels.
    // opaque:         indices into RendererState::draws drawn in the scope view, front to back.
    // blended:        alpha-blended ones, back to front.
    struct PreparedScope
    {
        bool active{false};
        math::Mat4 viewProjection{1.0f};
        uint32_t texels{0};
        std::vector<uint32_t> opaque;
        std::vector<uint32_t> blended;
    };

    // Prepares the frame's scope view from the submitted ScopeView, if any and if the scope
    // settings allow it: finds and marks the lens draw (no lens draw this frame means no scope
    // view), culls the world draws seen by the scope and writes the scope fields of the lighting
    // block. Call after buildDrawLists().
    // state: the renderer state with this frame's draws, submitted scope and prepared lighting.
    void prepareScope(RendererState& state);

    // Prepares the frame's holographic sight from the submitted HolographicSight, if any and if
    // RenderSettings::scope::holographic allows it: finds and marks its window draw (no window
    // draw this frame means no reticle) and writes the sight fields of the lighting block.
    // state: the renderer state with this frame's draws, submitted sight and prepared lighting.
    void prepareSight(RendererState& state);

    // Applies RenderSettings::scope::resolution (clamped) to the scope view attachments,
    // rebuilding them when it changed.
    // state:   the renderer state.
    // Returns: success, or an error if the graph rejected the new attachments (the old size stays).
    [[nodiscard]] Status updateScopeResolution(RendererState& state);

    // Renders the world seen through the frame's scope, from the scope's objective, into the
    // square scope colour target (lit colour with ambient included and the scope's fog; alpha 0
    // where nothing was drawn, so the lens shows the sky there). It shares the lit pass's
    // attachment formats so the PBR pipelines are shared too. Draws nothing when no scope is active.
    class ScopePass final : public vela::graphics::Pass
    {
    public:
        // state:   the renderer state the pass reads every frame. Must outlive the pass.
        // Returns: the pass.
        [[nodiscard]] static Result<std::unique_ptr<ScopePass>> create(RendererState& state);

        // Returns: the scope colour, ambient, velocity, G-buffer and depth outputs (cleared) at
        //          RendererState::scopeResolution; the shadow and environment atlases as inputs.
        [[nodiscard]] vela::graphics::PassDescription describe() const override;

        // Draws the scope view's draw lists.
        // recorder: Vela's recorder for this pass.
        void record(vela::graphics::PassRecorder& recorder) override;

    private:
        explicit ScopePass(const RendererState& state);

        const RendererState* m_state{nullptr};
    };
} //namespace lockstep::render

#endif //LOCKSTEP_RENDER_SCOPE_VIEW_HPP
