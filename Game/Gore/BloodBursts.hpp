#ifndef LOCKSTEP_GAME_GORE_BLOOD_BURSTS_HPP
#define LOCKSTEP_GAME_GORE_BLOOD_BURSTS_HPP

#include <Lockstep/Core/Math.hpp>
#include <Lockstep/Render/Particles.hpp>
#include <Lockstep/Render/Resources.hpp>

#include <cstdint>

namespace game
{
    namespace math = lockstep::math;

    // How the particle blood of a hit looks, editable from the dev panel. Presentation only.
    // isEnabled:       particle blood is spawned at all.
    // amount:          factor on particle counts.
    // size:            factor on particle sizes.
    // exitShare:       factor on the exit spray (out of the back along the shot).
    // headshotScale:   factor on the mist of a head hit (it also throws fragments).
    // referenceDamage: damage of a full-strength hit (the player's shot); weaker hits spray less.
    // bodyDepth:       how far behind the entry point the exit spray starts, metres.
    struct BloodBurstTuning
    {
        bool isEnabled{true};
        float amount{1.0f};
        float size{1.0f};
        float exitShare{1.0f};
        float headshotScale{1.8f};
        float referenceDamage{34.0f};
        float bodyDepth{0.28f};
    };

    // The blood particle effects, registered in the game's particle system on first use.
    // droplets:  dense lit droplets streaking under gravity.
    // mist:      a short red mist puff.
    // fragments: small dark bits thrown by a head hit.
    // isTried:   registering was attempted (not retried after a failure).
    // isReady:   the effects are registered.
    struct BloodBurstEffects
    {
        lockstep::render::ParticleEffectId droplets{};
        lockstep::render::ParticleEffectId mist{};
        lockstep::render::ParticleEffectId fragments{};
        bool isTried{false};
        bool isReady{false};
    };

    // One hit to show.
    // point:      world point it struck (the entry wound).
    // direction:  unit direction the shot travelled.
    // damage:     hit points it took (0 if unknown: counts as referenceDamage).
    // isHeadshot: it struck the head.
    // isCorpse:   it struck a dead body (a weaker spray).
    struct BloodBurstHit
    {
        math::Vec3 point{0.0f, 0.0f, 0.0f};
        math::Vec3 direction{0.0f, 0.0f, -1.0f};
        int32_t damage{0};
        bool isHeadshot{false};
        bool isCorpse{false};
    };

    // Returns how strong a hit looks: its damage over the reference, clamped.
    // tuning:  the look.
    // damage:  hit points (0 if unknown).
    // Returns: about 0.3 for a light hit, 1 for a reference hit, up to 2.
    [[nodiscard]] float bloodStrength(const BloodBurstTuning& tuning, int32_t damage);

    // Spawns a hit's particle blood: droplets and a mist puff blown back out of the entry, a
    // stronger spray and mist out of the exit along the shot, fragments for a head hit. Registers
    // the effects on first use. Does nothing without a particle system.
    // effects:   the blood effects.
    // particles: the game's particle system (null if unavailable).
    // atlas:     the game's particle sprite atlas.
    // tuning:    the look.
    // hit:       the hit.
    void spawnBloodBurst(BloodBurstEffects& effects, lockstep::render::ParticleSystem* particles,
        lockstep::render::TextureHandle atlas, const BloodBurstTuning& tuning, const BloodBurstHit& hit);

#if LOCKSTEP_DEV_TOOLS
    // Draws the blood burst controls of the Game dev panel.
    // tuning: the look.
    void bloodBurstDevSection(BloodBurstTuning& tuning);
#endif
} //namespace game

#endif //LOCKSTEP_GAME_GORE_BLOOD_BURSTS_HPP
