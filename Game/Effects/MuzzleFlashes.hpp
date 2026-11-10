#ifndef LOCKSTEP_GAME_EFFECTS_MUZZLE_FLASHES_HPP
#define LOCKSTEP_GAME_EFFECTS_MUZZLE_FLASHES_HPP

#include "Weapons/WeaponSockets.hpp"

#include <Lockstep/Core/Math.hpp>
#include <Lockstep/Render/Particles.hpp>
#include <Lockstep/Render/Resources.hpp>

#include <cstdint>
#include <unordered_map>
#include <vector>

namespace lockstep::render
{
    class Renderer;
} //namespace lockstep::render

namespace game
{
    // Switches and amounts of the muzzle effects, editable from the dev panel.
    // flashes:        flash particles (front star, side flame, sparks).
    // lights:         the flash's point light.
    // tracers:        tracer streaks.
    // tracerEvery:    every this many rounds of a shooter is a tracer.
    // flashSize:      factor on the size of the world flash particles (partner and NPCs).
    // flashIntensity: factor on their HDR colour (keep it low: bloom and auto exposure react).
    // lightIntensity, lightRange: the flash light.
    // lightVolumetric: how strongly the flash lights fog and haze.
    // ownLightScale:  factor on the light of the local weapon (it sits next to the camera, so it
    //                 lights the weapon and the fog in front of the eye far more than others').
    // ownLightVolumetric: fog factor of the local weapon's light (fog right at the eye would
    //                 wash out the whole screen).
    // ownLightAhead:  metres ahead of the local muzzle the light is placed.
    // ownSmoke:       factor on the size, amount and opacity of the local weapon's shot smoke (it
    //                 hangs right in front of the eye and the weapon light).
    // lightFrames:    frames the light shows at least (it also lasts lightSeconds of World time,
    //                 so bullet-time stretches it).
    // lightSeconds:   World seconds the light shows at least.
    // heatPerShot:    barrel heat a shot adds.
    // heatDecay:      heat lost per World second, as a share of the heat.
    // maxHeat:        most heat a barrel holds.
    // smokeHeat:      heat from which the barrel keeps smoking between shots.
    // smokeRate:      barrel smoke wisps per second per unit of heat above smokeHeat.
    struct MuzzleFlashTuning
    {
        bool flashes{true};
        bool lights{true};
        bool tracers{true};
        int tracerEvery{4};
        float flashSize{0.6f};
        float flashIntensity{0.3f};
        float lightIntensity{22.0f};
        float lightRange{4.0f};
        float lightVolumetric{0.6f};
        float ownLightScale{0.35f};
        float ownLightVolumetric{0.15f};
        float ownLightAhead{0.5f};
        float ownSmoke{0.25f};
        int lightFrames{1};
        float lightSeconds{0.02f};
        float heatPerShot{1.0f};
        float heatDecay{0.45f};
        float maxHeat{14.0f};
        float smokeHeat{3.0f};
        float smokeRate{2.5f};
    };

    // One flash light still showing.
    // position: where.
    // volumetric: its fog factor.
    // frames:   frames shown so far.
    // seconds:  World seconds since the shot.
    // strength: random brightness factor of this shot.
    struct MuzzleLight
    {
        math::Vec3 position{0.0f, 0.0f, 0.0f};
        float volumetric{1.0f};
        int frames{0};
        float seconds{0.0f};
        float strength{1.0f};
    };

    // A shooter's barrel: its heat and the smoke still owed to it.
    // heat:  rises with every shot, decays over World time.
    // carry: fractional smoke wisps not spawned yet.
    struct BarrelHeat
    {
        float heat{0.0f};
        float carry{0.0f};
    };

    // Muzzle flashes, flash lights, smoke building up over sustained fire, and tracers.
    // Presentation only, spawned from shots this machine already knows about; the particles live
    // in Effects' particle system (World time, never in mirrors).
    // flashCore, flashSide, flashSparks, tracer, barrelSmoke: the effects, once created.
    // isTried:  creating the effects was attempted.
    // lights:   lights still showing.
    // barrels:  per shooter key, its barrel heat.
    // tuning:   switches and amounts.
    // random:   presentation-only random state.
    struct MuzzleFlashes
    {
        lockstep::render::ParticleEffectId flashCore{};
        lockstep::render::ParticleEffectId flashSide{};
        lockstep::render::ParticleEffectId flashSparks{};
        lockstep::render::ParticleEffectId tracer{};
        lockstep::render::ParticleEffectId barrelSmoke{};
        bool isTried{false};
        std::vector<MuzzleLight> lights{};
        std::unordered_map<uint32_t, BarrelHeat> barrels{};
        MuzzleFlashTuning tuning{};
        uint32_t random{0x1B873593u};
    };

    // Registers the muzzle effects once.
    // muzzle:    the muzzle effects.
    // particles: the particle system.
    // atlas:     the sprite atlas.
    // Returns:   true when the effects exist.
    bool createMuzzleEffects(MuzzleFlashes& muzzle, lockstep::render::ParticleSystem& particles,
        lockstep::render::TextureHandle atlas);

    // Flashes a muzzle for one shot: flash particles (different every shot), a short light, a smoke
    // puff that grows with the barrel's heat.
    // muzzle:    the muzzle effects.
    // particles: the particle system, or null (only the light and heat then).
    // socket:    the weapon as drawn (world space, where it appears on screen).
    // shooter:   the shooter's key (its barrel heat).
    // smoke:     the per-shot smoke effect (EffectIds::muzzleSmoke), or an invalid id for none.
    // isOwn:     the local first-person weapon: no flash particles (its flash is drawn with the
    //            viewmodel, ViewModelFlash) and the dimmer own light further ahead.
    // strength:  factor on the light (less while aiming down sights).
    void flashMuzzle(MuzzleFlashes& muzzle, lockstep::render::ParticleSystem* particles, const WeaponSocket& socket,
        uint32_t shooter, lockstep::render::ParticleEffectId smoke, bool isOwn, float strength);

    // Spawns a tracer streak flying from the muzzle to where the round hits.
    // muzzle:    the muzzle effects.
    // particles: the particle system.
    // from:      the muzzle.
    // to:        the hit point (or a far point along the shot).
    void fireTracer(MuzzleFlashes& muzzle, lockstep::render::ParticleSystem& particles, const math::Vec3& from,
        const math::Vec3& to);

    // Lets a hot barrel smoke between shots.
    // muzzle:    the muzzle effects.
    // particles: the particle system.
    // shooter:   the shooter's key.
    // socket:    its weapon as drawn this frame.
    // isOwn:     the local first-person weapon (thinner, smaller wisps: ownSmoke).
    // seconds:   World seconds since the previous frame.
    void smokeBarrel(MuzzleFlashes& muzzle, lockstep::render::ParticleSystem& particles, uint32_t shooter,
        const WeaponSocket& socket, bool isOwn, float seconds);

    // Cools the barrels and submits the flash lights still showing (and forgets finished ones).
    // muzzle:   the muzzle effects.
    // renderer: the renderer.
    // seconds:  World seconds since the previous frame.
    void updateMuzzleFlashes(MuzzleFlashes& muzzle, lockstep::render::Renderer& renderer, float seconds);
} //namespace game

#endif //LOCKSTEP_GAME_EFFECTS_MUZZLE_FLASHES_HPP
