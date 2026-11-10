#ifndef LOCKSTEP_RENDER_PARTICLE_PASS_HPP
#define LOCKSTEP_RENDER_PARTICLE_PASS_HPP

#include "Lockstep/Core/Result.hpp"

#include "FrameParticles.hpp"

#include <Vela/Graphics/DynamicBuffer.hpp>
#include <Vela/Graphics/Material.hpp>
#include <Vela/Graphics/Pass.hpp>

#include <cstdint>
#include <memory>
#include <vector>

namespace lockstep::render
{
    struct RendererState;

    // Draws the frame's particles (RendererState::particles) over the lit scene, after the lit
    // scene pass and before post-processing. Loads the HDR colour and ambient targets and samples
    // the depth buffer (soft particles, manual depth test), the shadow atlas and the environment
    // atlas (lighting). Alpha-blended particles are sorted back to front and drawn first, additive
    // ones after them; each run of particles sharing a texture is one instanced draw. Alpha
    // blending also scales the ambient target by 1 - alpha, so smoke covers the SSAO-composited
    // ambient light behind it as well.
    class ParticlePass final : public vela::graphics::Pass
    {
    public:
        // Loads the particle shaders and creates the alpha and additive materials and the
        // instance buffer.
        // state:   the renderer state whose particles, light buffer and textures the pass reads
        //          every frame. Must outlive the pass.
        // Returns: the pass, or an error if a shader is missing or GPU creation failed.
        [[nodiscard]] static Result<std::unique_ptr<ParticlePass>> create(RendererState& state);

        // Returns: colour and ambient (loaded); depth, shadow atlas and environment atlas sampled.
        [[nodiscard]] vela::graphics::PassDescription describe() const override;

        // Sorts and uploads this frame's particles and draws them.
        // recorder: Vela's recorder for this pass.
        void record(vela::graphics::PassRecorder& recorder) override;

    private:
        // A run of uploaded particles drawn with one texture and blend mode.
        struct DrawRun
        {
            TextureHandle texture{};
            bool isAdditive{false};
            uint32_t first{0};
            uint32_t count{0};
        };

        ParticlePass(const RendererState& state, vela::graphics::Material&& alphaMaterial,
            vela::graphics::Material&& additiveMaterial, vela::graphics::DynamicBuffer&& instances);

        // Fills m_upload and m_runs from this frame's particles: alpha-blended ones sorted back to
        // front, then additive ones.
        void buildDrawList();

        const RendererState* m_state{nullptr};
        vela::graphics::Material m_alphaMaterial;
        vela::graphics::Material m_additiveMaterial;
        vela::graphics::DynamicBuffer m_instances;
        std::vector<GpuParticle> m_upload;
        std::vector<uint64_t> m_sortKeys;
        std::vector<DrawRun> m_runs;
    };
} //namespace lockstep::render

#endif //LOCKSTEP_RENDER_PARTICLE_PASS_HPP
