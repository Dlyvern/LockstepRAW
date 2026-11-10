#ifndef LOCKSTEP_RENDER_FRAME_PARTICLES_HPP
#define LOCKSTEP_RENDER_FRAME_PARTICLES_HPP

#include "Lockstep/Render/Resources.hpp"

#include <cstdint>
#include <vector>

namespace lockstep::render
{
    // Flag bits of GpuParticle::params[3]. Must match particle.vert.
    inline constexpr uint32_t k_particleLit{1u};
    inline constexpr uint32_t k_particleAdditive{2u};

    // One particle as the particle shaders read it (per-instance vertex data, 80 bytes).
    // positionSize:    world position, diameter in metres.
    // velocityStretch: world velocity in m/s, seconds of motion a streak covers (0: billboard).
    // color:           linear RGB (albedo when lit), opacity.
    // params:          rotation in radians, atlas frame (fraction blends into the next frame),
    //                  softness in metres, flag bits (k_particleLit, k_particleAdditive).
    // atlas:           atlas columns, atlas rows, forward scattering, unused.
    struct GpuParticle
    {
        float positionSize[4]{};
        float velocityStretch[4]{};
        float color[4]{};
        float params[4]{};
        float atlas[4]{};
    };

    static_assert(sizeof(GpuParticle) == 80);

    // Particles of one effect handed to the renderer, a range of FrameParticles::particles.
    // texture:    the effect's sprite texture.
    // isAdditive: blended additively (else alpha-blended and depth-sorted).
    // first:      index of its first particle.
    // count:      number of particles.
    struct ParticleBatch
    {
        TextureHandle texture{};
        bool isAdditive{false};
        uint32_t first{0};
        uint32_t count{0};
    };

    // The particles the particle pass draws this frame (ParticleSystem::submit()).
    // particles:  every particle, grouped by batch.
    // batches:    the groups.
    // frameIndex: RendererState::frameIndex they were submitted for; older data is not drawn.
    struct FrameParticles
    {
        std::vector<GpuParticle> particles;
        std::vector<ParticleBatch> batches;
        uint32_t frameIndex{0};
    };
} //namespace lockstep::render

#endif //LOCKSTEP_RENDER_FRAME_PARTICLES_HPP
