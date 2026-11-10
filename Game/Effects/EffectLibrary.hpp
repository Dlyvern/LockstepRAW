#ifndef LOCKSTEP_GAME_EFFECTS_EFFECT_LIBRARY_HPP
#define LOCKSTEP_GAME_EFFECTS_EFFECT_LIBRARY_HPP

#include <Lockstep/Core/Result.hpp>
#include <Lockstep/Render/Particles.hpp>

namespace game
{
    // The game's particle effects, all drawn from the procedural sprite atlas.
    // dustMotes:    tiny lit motes drifting around the camera indoors; they show where light falls.
    // impactPuff:   a dust puff blown off a wall by a bullet.
    // impactGrit:   fine grit and plaster dust falling from an impact.
    // impactChunks: small debris chips thrown from an impact (tinted per surface).
    // sparks:       hot streaks off metal.
    // muzzleSmoke:  a thin wisp left at a muzzle by a shot.
    // steam:        a rising white plume (Fx_Steam_* markers).
    // smoke:        a slow dark smoke column (Fx_Smoke_* markers).
    // dustFall:     a trickle of dust falling from the ceiling (Fx_Dust_* markers).
    // stress:       large smoke puffs for the dev panel's cost test.
    // rainStreaks:  falling rain, thin additive streaks lit by the sun, sky and local lights (they
    //               glint towards lamps); they die inside interior boxes.
    // rainSplash:   a few droplets thrown up where a raindrop hits the ground.
    struct EffectIds
    {
        lockstep::render::ParticleEffectId dustMotes{};
        lockstep::render::ParticleEffectId impactPuff{};
        lockstep::render::ParticleEffectId impactGrit{};
        lockstep::render::ParticleEffectId impactChunks{};
        lockstep::render::ParticleEffectId sparks{};
        lockstep::render::ParticleEffectId muzzleSmoke{};
        lockstep::render::ParticleEffectId steam{};
        lockstep::render::ParticleEffectId smoke{};
        lockstep::render::ParticleEffectId dustFall{};
        lockstep::render::ParticleEffectId stress{};
        lockstep::render::ParticleEffectId rainStreaks{};
        lockstep::render::ParticleEffectId rainSplash{};
    };

    // Returns: the rain streak effect's stretch at normal speed, in seconds of motion.
    [[nodiscard]] float rainStreakStretch();

    // Registers every game effect in a particle system.
    // particles: the particle system.
    // atlas:     the sprite atlas texture (effectAtlasPixels()).
    // Returns:   the effect ids, or an error if a description was rejected.
    [[nodiscard]] lockstep::Result<EffectIds> createEffects(lockstep::render::ParticleSystem& particles,
        lockstep::render::TextureHandle atlas);
} //namespace game

#endif //LOCKSTEP_GAME_EFFECTS_EFFECT_LIBRARY_HPP
