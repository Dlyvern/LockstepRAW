#ifndef LOCKSTEP_RENDER_PLANAR_REFLECTIONS_HPP
#define LOCKSTEP_RENDER_PLANAR_REFLECTIONS_HPP

#include "Lockstep/Core/Math.hpp"
#include "Lockstep/Core/Result.hpp"
#include "Lockstep/Render/Views.hpp"

#include <Vela/Graphics/Pass.hpp>

#include <cstdint>
#include <memory>
#include <vector>

namespace lockstep::render
{
    struct RendererState;

    // The planar reflection chosen for one frame.
    // active:         a reflector is rendered this frame.
    // viewProjection: world to clip of the reflection view: the camera mirrored by the plane, an
    //                 oblique near plane on the mirror and a horizontal flip that restores the
    //                 triangle winding the mirroring reversed.
    // cameraPosition: the mirrored camera position (for shading).
    // plane:          xyz world normal towards the camera, w minus the offset along it.
    // tint:           rgb multiplier on the reflection.
    // layers:         the render layers the reflection shows.
    // clipRect:       x0, y0, x1, y1 of the mirror's footprint in the reflection's normalized
    //                 device coordinates, for the scissor.
    // opaque:         indices into RendererState::draws drawn in the reflection, front to back.
    // blended:        alpha-blended ones, back to front.
    // distance:       distance from the camera to the reflector, metres.
    struct PreparedReflection
    {
        bool active{false};
        math::Mat4 viewProjection{1.0f};
        math::Vec3 cameraPosition{0.0f, 0.0f, 0.0f};
        math::Vec4 plane{0.0f, 0.0f, 0.0f, 0.0f};
        math::Vec3 tint{1.0f, 1.0f, 1.0f};
        RenderLayers layers{k_defaultRenderLayer};
        math::Vec4 clipRect{-1.0f, -1.0f, 1.0f, 1.0f};
        std::vector<uint32_t> opaque;
        std::vector<uint32_t> blended;
        float distance{0.0f};
    };

    // Picks the frame's planar reflection: the nearest submitted reflector that is in front of the
    // camera, inside its frustum and within RenderSettings::reflections.maxDistance. Derives its
    // plane and view, marks its draw as the reflector surface, culls the draws seen through it and
    // writes the reflection fields of the lighting block.
    // state: the renderer state with this frame's draws, reflectors, view and unjittered
    //        projection.
    void prepareReflection(RendererState& state);

    // Renders the scene seen in the frame's planar reflection, from the mirrored camera, into the
    // reduced-resolution reflection colour target (lit colour with ambient included, no SSAO). It
    // shares the lit pass's attachment formats so the PBR pipelines are shared too. Only the
    // mirror's footprint is drawn (scissor), and nothing when no reflector is active.
    class ReflectionPass final : public vela::graphics::Pass
    {
    public:
        // state:   the renderer state the pass reads every frame. Must outlive the pass.
        // Returns: the pass.
        [[nodiscard]] static Result<std::unique_ptr<ReflectionPass>> create(RendererState& state);

        // Returns: the reflection colour, ambient, velocity, G-buffer and depth outputs (cleared) at
        //          RendererState::reflectionScale; the shadow and environment atlases as inputs.
        [[nodiscard]] vela::graphics::PassDescription describe() const override;

        // Draws the reflection's draw lists.
        // recorder: Vela's recorder for this pass.
        void record(vela::graphics::PassRecorder& recorder) override;

    private:
        explicit ReflectionPass(const RendererState& state);

        const RendererState* m_state{nullptr};
    };
} //namespace lockstep::render

#endif //LOCKSTEP_RENDER_PLANAR_REFLECTIONS_HPP
