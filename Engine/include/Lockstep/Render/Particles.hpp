#ifndef LOCKSTEP_RENDER_PARTICLES_HPP
#define LOCKSTEP_RENDER_PARTICLES_HPP

#include "Lockstep/Core/Bounds.hpp"
#include "Lockstep/Core/Math.hpp"
#include "Lockstep/Core/Result.hpp"
#include "Lockstep/Core/Transform.hpp"
#include "Lockstep/Render/Resources.hpp"

#include <cstdint>
#include <limits>
#include <memory>
#include <span>

namespace lockstep::render
{
    class Renderer;
    struct ParticleSystemState;

    // How a particle is blended over the scene.
    // Alpha:    covers what is behind by its alpha (smoke, dust, debris). Sorted back to front.
    // Additive: adds its colour times alpha (sparks, embers, glints). Order-independent.
    enum class ParticleBlend : uint8_t
    {
        Alpha = 0,
        Additive
    };

    // How a particle's colour responds to light.
    // Lit:   the colour is an albedo lit by the sun (with shadows), the local lights, the sky's
    //        ambient light (dimmed inside interior zones) and fogged; evaluated once per particle.
    // Unlit: the colour is emitted as is (HDR values above 1 glow through bloom); only fog fades it.
    enum class ParticleLighting : uint8_t
    {
        Lit = 0,
        Unlit
    };

    // The quad a particle is drawn as.
    // Billboard: a square facing the camera, turned by the particle's rotation.
    // Stretched: a streak along the particle's screen-space motion, as long as the distance it
    //            travels in ParticleEffectDescription::stretch seconds (sparks, rain).
    enum class ParticleShape : uint8_t
    {
        Billboard = 0,
        Stretched
    };

    // A value over a particle's life: `start` at birth, `middle` at `middleTime` (0..1 of the
    // life), `end` at death, linear in between.
    template<typename T>
    struct ParticleCurve
    {
        T start{};
        T middle{};
        T end{};
        float middleTime{0.5f};
    };

    // Everything that defines how one kind of particle looks and moves. Ranges are (min, max)
    // and picked uniformly per particle.
    // texture:      sprite texture created by the Renderer, straight alpha. One atlas shared by
    //               many effects keeps the draw count down.
    // atlasColumns, atlasRows: the texture's grid of equally sized frames, row-major from the
    //               top-left.
    // firstFrame, frameCount: the frames this effect uses.
    // isFlipbook:   true plays the frames over the particle's life (blended); false picks one at
    //               random per particle.
    // blend, lighting, shape: see the enums.
    // spawnRate:    particles per second an emitter of this effect spawns at rate scale 1.
    // burstCount:   particles one burst() spawns at count scale 1.
    // lifetime:     seconds a particle lives.
    // speed:        initial speed in m/s along a direction inside the cone.
    // coneAngle:    half angle in radians around the emit direction (pi: all directions).
    // spawnExtent:  half size of the box particles start in, in the emitter's space (scaled by
    //               the emitter's scale); zero spawns at the point.
    // acceleration: constant acceleration in m/s^2 (gravity, buoyancy, wind).
    // drag:         fraction of velocity lost per second (exponential); 0 for none.
    // jitter:       random acceleration in m/s^2, changing every update, for wandering motes.
    // size:         diameter in metres over life (width of a streak).
    // sizeScale:    random factor on `size` per particle.
    // color:        linear RGB (above 1 glows when unlit) and opacity over life.
    // spin:         rotation speed in rad/s (random sign); billboards also start at a random angle.
    // stretch:      Stretched only: seconds of motion the streak covers.
    // softness:     metres over which the particle fades in front of opaque surfaces (soft
    //               particles); 0 for a hard intersection.
    // scattering:   Lit only: forward scattering, 0 (even) .. 0.9 (bright when looking towards a
    //               light), for dust in light beams.
    // diesInExclusionBoxes: a particle dies as soon as it is inside one of the system's exclusion
    //               boxes (ParticleSystem::setExclusionBoxes), e.g. rain reaching a roof.
    struct ParticleEffectDescription
    {
        TextureHandle texture{};
        uint32_t atlasColumns{1};
        uint32_t atlasRows{1};
        uint32_t firstFrame{0};
        uint32_t frameCount{1};
        bool isFlipbook{false};
        ParticleBlend blend{ParticleBlend::Alpha};
        ParticleLighting lighting{ParticleLighting::Lit};
        ParticleShape shape{ParticleShape::Billboard};
        float spawnRate{0.0f};
        uint32_t burstCount{0};
        math::Vec2 lifetime{1.0f, 1.0f};
        math::Vec2 speed{0.0f, 0.0f};
        float coneAngle{0.0f};
        math::Vec3 spawnExtent{0.0f, 0.0f, 0.0f};
        math::Vec3 acceleration{0.0f, 0.0f, 0.0f};
        float drag{0.0f};
        float jitter{0.0f};
        ParticleCurve<float> size{0.1f, 0.1f, 0.1f, 0.5f};
        math::Vec2 sizeScale{1.0f, 1.0f};
        ParticleCurve<math::Vec4> color{{1.0f, 1.0f, 1.0f, 1.0f}, {1.0f, 1.0f, 1.0f, 1.0f},
            {1.0f, 1.0f, 1.0f, 0.0f}, 0.5f};
        math::Vec2 spin{0.0f, 0.0f};
        float stretch{0.0f};
        float softness{0.1f};
        float scattering{0.0f};
        bool diesInExclusionBoxes{false};
    };

    // Identifies an effect of one ParticleSystem. Effects live as long as their system.
    struct ParticleEffectId
    {
        uint32_t index{std::numeric_limits<uint32_t>::max()};

        // Returns: true if the id was returned by createEffect().
        [[nodiscard]] constexpr bool isValid() const noexcept
        {
            return index != std::numeric_limits<uint32_t>::max();
        }
    };

    // Identifies an emitter of one ParticleSystem. Stale once the emitter is destroyed.
    struct ParticleEmitterId
    {
        uint32_t index{std::numeric_limits<uint32_t>::max()};
        uint32_t generation{0};

        // Returns: true if the id was ever returned by createEmitter() (it may be stale).
        [[nodiscard]] constexpr bool isValid() const noexcept
        {
            return index != std::numeric_limits<uint32_t>::max();
        }
    };

    // One burst of particles at a point.
    // position:   where they start (plus the effect's spawnExtent around it).
    // direction:  the cone axis, usually a surface normal; normalized by burst().
    // countScale: factor on the effect's burstCount (fractions round randomly).
    // sizeScale:  factor on the particles' size.
    // tint:       factor on the effect's colour and opacity (e.g. a surface's dust colour).
    // velocity:   added to every particle's initial velocity (a moving source).
    struct ParticleBurst
    {
        math::Vec3 position{0.0f, 0.0f, 0.0f};
        math::Vec3 direction{0.0f, 1.0f, 0.0f};
        float countScale{1.0f};
        float sizeScale{1.0f};
        math::Vec4 tint{1.0f, 1.0f, 1.0f, 1.0f};
        math::Vec3 velocity{0.0f, 0.0f, 0.0f};
    };

    // Largest particle budget a ParticleSystem accepts.
    inline constexpr uint32_t k_maxParticleBudget{1u << 20};

    // Budget of a ParticleSystem.
    // maxParticles: most particles alive at once (1 .. k_maxParticleBudget); spawns beyond it are
    //               dropped (and counted).
    struct ParticleSystemDescription
    {
        uint32_t maxParticles{20000};
    };

    // Counters for the dev tools.
    // alive:              particles alive after the last update().
    // emitters:           living emitters.
    // dropped:            spawns dropped over the budget since creation.
    // updateMilliseconds: wall time of the last update().
    // submitMilliseconds: wall time of the last submit().
    struct ParticleStats
    {
        uint32_t alive{0};
        uint32_t emitters{0};
        uint64_t dropped{0};
        float updateMilliseconds{0.0f};
        float submitMilliseconds{0.0f};
    };

    // CPU-simulated, camera-facing particles drawn in the lit HDR scene after opaque geometry and
    // decals, with soft edges against the depth buffer, in at most a few instanced draws. They are
    // drawn only in the camera view: never in planar reflections or shadow maps, and they write
    // no depth or motion vectors. Presentation only: the simulation never touches game state, and
    // the caller supplies the delta time (pass a scaled one to slow particles in bullet-time).
    // Each frame: spawn (burst(), emitters), update(), then submit() before Renderer::render().
    class ParticleSystem
    {
    public:
        // Creates an empty particle system.
        // description: the particle budget.
        // Returns:     the system, or an error if the budget is 0 or above k_maxParticleBudget.
        [[nodiscard]] static Result<ParticleSystem> create(const ParticleSystemDescription& description);

        ~ParticleSystem();

        ParticleSystem(ParticleSystem&& other) noexcept;
        ParticleSystem& operator=(ParticleSystem&& other) noexcept;

        ParticleSystem(const ParticleSystem&) = delete;
        ParticleSystem& operator=(const ParticleSystem&) = delete;

        // Registers an effect.
        // description: the effect; copied.
        // Returns:     its id, or an error if the atlas grid, frame range, lifetime or sizes are
        //              invalid (the texture is checked when drawn: an invalid one hides the effect).
        [[nodiscard]] Result<ParticleEffectId> createEffect(const ParticleEffectDescription& description);

        // Replaces an effect's description; living particles take the new look and motion at once.
        // effect:      the effect.
        // description: the new description, validated like createEffect().
        // Returns:     an error if the id is unknown or the description invalid (nothing changes).
        [[nodiscard]] Status setEffect(ParticleEffectId effect, const ParticleEffectDescription& description);

        // Returns: an effect's description, or null for an unknown id.
        [[nodiscard]] const ParticleEffectDescription* findEffect(ParticleEffectId effect) const;

        // Creates an emitter that spawns `effect` continuously at its spawnRate, along the
        // transform's +Y axis.
        // effect:    the effect it spawns.
        // transform: where it is; its rotation turns the emit cone and spawn box, its scale the box.
        // Returns:   the emitter, or an error for an unknown effect.
        [[nodiscard]] Result<ParticleEmitterId> createEmitter(ParticleEffectId effect, const math::Transform& transform);

        // Moves an emitter. Stale ids are ignored.
        // emitter:   the emitter.
        // transform: its new placement.
        void setEmitterTransform(ParticleEmitterId emitter, const math::Transform& transform);

        // Changes how fast an emitter spawns. Stale ids are ignored.
        // emitter: the emitter.
        // scale:   factor on the effect's spawnRate; 0 pauses it.
        void setEmitterRate(ParticleEmitterId emitter, float scale);

        // Destroys an emitter; the particles it spawned live out their lives.
        // emitter: the emitter. Stale ids are ignored.
        // Returns: true if a living emitter was destroyed.
        bool destroyEmitter(ParticleEmitterId emitter);

        // Spawns a burst of an effect's particles at once.
        // effect:  the effect.
        // burst:   where, which way and how many.
        // Returns: the number of particles spawned (fewer than asked when over budget, 0 for an
        //          unknown effect).
        uint32_t burst(ParticleEffectId effect, const ParticleBurst& burst);

        // Spawns from the emitters and advances every particle.
        // seconds: delta time to simulate; negative values count as 0.
        void update(float seconds);

        // Sets the boxes particles of effects with diesInExclusionBoxes die in (checked every
        // update()), replacing earlier ones.
        // boxes: world-space boxes; copied. Empty for none.
        void setExclusionBoxes(std::span<const math::Aabb> boxes);

        // Removes every particle and emitter (effects stay), e.g. on a level switch.
        void clear();

        // Hands the living particles to the renderer for its next render(), replacing what an
        // earlier submit() handed it this frame. Call after update() and before render().
        // renderer: the renderer whose textures the effects use.
        void submit(Renderer& renderer);

        // Returns: counters for the dev tools.
        [[nodiscard]] ParticleStats stats() const;

    private:
        explicit ParticleSystem(std::unique_ptr<ParticleSystemState> state);

        std::unique_ptr<ParticleSystemState> m_state;
    };
} //namespace lockstep::render

#endif //LOCKSTEP_RENDER_PARTICLES_HPP
