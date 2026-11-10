#ifndef LOCKSTEP_GAME_EFFECTS_EFFECTS_HPP
#define LOCKSTEP_GAME_EFFECTS_EFFECTS_HPP

#include "Effects/EffectLibrary.hpp"
#include "Weather/RainParticles.hpp"

#include <Lockstep/Core/Math.hpp>
#include <Lockstep/Render/Particles.hpp>
#include <Lockstep/Render/Resources.hpp>

#include <cstdint>
#include <optional>
#include <vector>

namespace lockstep::assets
{
    struct Scene;
} //namespace lockstep::assets

namespace lockstep::app
{
    struct EngineContext;
    struct GameState;
} //namespace lockstep::app

namespace game
{
    namespace math = lockstep::math;

    // Switches and scales for the particle effects, editable from the dev panel.
    // enabled:     draw and simulate particles at all.
    // dustMotes:   motes around the camera inside Zone_Interior_* boxes.
    // impacts:     dust, grit, chips and sparks at the local player's bullet impacts.
    // muzzleSmoke: smoke wisps at every gunshot (players and NPCs).
    // markers:     emitters on the level's Fx_* markers.
    // moteDensity: factor on the motes' spawn rate.
    // impactScale: factor on impact particle counts.
    // stressCount: particles the dev panel's stress test spawns around the camera.
    struct EffectsTuning
    {
        bool enabled{true};
        bool dustMotes{true};
        bool impacts{true};
        bool muzzleSmoke{true};
        bool markers{true};
        float moteDensity{1.0f};
        float impactScale{1.0f};
        int stressCount{20000};
    };

    // Particle effects: presentation only, never replicated, spawned from frame() and from combat
    // events the machine already has (CombatAudio's shots and impacts), simulated with the World
    // time domain's delta so they slow down in bullet-time.
    // particles:   the particle system, once created.
    // atlas:       the sprite atlas texture.
    // ids:         the registered effects.
    // isTried:     creation was attempted (it is not retried after a failure).
    // motes:       the dust mote emitter that follows the camera.
    // wasIndoors:  the camera was inside an interior box last frame (motes fill in on entering).
    // rain:        the rain's streak emitter and splashes (driven by the weather).
    // markers:     emitters on the current level's Fx_* markers.
    // scene:       the scene the markers came from.
    // tuning:      switches and scales.
    // eye:         the camera position last frame (the stress test spawns around it).
    // random:      presentation-only random state.
    struct Effects
    {
        std::optional<lockstep::render::ParticleSystem> particles{};
        lockstep::render::TextureHandle atlas{};
        EffectIds ids{};
        bool isTried{false};
        std::optional<lockstep::render::ParticleEmitterId> motes{};
        bool wasIndoors{false};
        RainParticles rain{};
        std::vector<lockstep::render::ParticleEmitterId> markers{};
        const lockstep::assets::Scene* scene{nullptr};
        EffectsTuning tuning{};
        math::Vec3 eye{0.0f, 0.0f, 0.0f};
        uint32_t random{0x6C8E9CF5u};
    };

    // Spawns this frame's effects (from state.combatAudio's shots and impacts, the camera, the
    // level's Fx_* markers and the weather's rain), simulates them and submits them to the renderer.
    // Call after the combat audio and the weather have run this frame.
    // state:        the game state (effects, level, combat events, interior boxes).
    // engine:       the engine (renderer, time domains).
    // eye:          the camera position.
    // aim:          the local view direction (the own muzzle).
    // worldSeconds: World time since the previous frame (scaled by bullet-time).
    // frameSeconds: real time since the previous frame.
    void presentEffects(lockstep::app::GameState& state, lockstep::app::EngineContext& engine, const math::Vec3& eye,
        const math::Vec3& aim, float worldSeconds, float frameSeconds);

#if LOCKSTEP_DEV_TOOLS
    // Draws the effects section of the Game dev panel: switches, scales, counters and a stress test.
    // effects: the effects state.
    void effectsDevSection(Effects& effects);
#endif
} //namespace game

#endif //LOCKSTEP_GAME_EFFECTS_EFFECTS_HPP
