#ifndef LOCKSTEP_GAME_EFFECTS_DRIPS_HPP
#define LOCKSTEP_GAME_EFFECTS_DRIPS_HPP

#include <Lockstep/Audio/AudioTypes.hpp>
#include <Lockstep/Core/Math.hpp>
#include <Lockstep/Render/Particles.hpp>
#include <Lockstep/Render/Resources.hpp>

#include <cstdint>
#include <vector>

namespace lockstep::app
{
    struct EngineContext;
    struct GameState;
} //namespace lockstep::app

namespace lockstep::assets
{
    struct Scene;
} //namespace lockstep::assets

namespace game
{
    namespace math = lockstep::math;

    // Look and timing of the ceiling drips, editable from the dev panel.
    // enabled:      drip (also needs GraphicsQuality::waterRipples).
    // interval:     average World seconds between drops of one leak.
    // jitter:       random share of the interval, 0 (a metronome) to 1.
    // puddleRadius: radius of the puddle under a leak, metres.
    // reach:        leaks farther than this from the camera are paused, metres.
    // volume:       the drop sound.
    struct DripTuning
    {
        bool enabled{true};
        float interval{1.6f};
        float jitter{0.6f};
        float puddleRadius{0.3f};
        float reach{18.0f};
        float volume{0.45f};
    };

    // One leak: an Fx_Drip_* marker on a ceiling and the floor below it.
    // source:      where drops form (the marker).
    // floor:       where they land (a ray straight down).
    // fallSeconds: how long a drop falls.
    // untilDrop:   World seconds until the next drop lets go.
    // untilLanding: World seconds until the falling drop lands (negative: none falling).
    // dropAge:     World seconds since the last drop landed.
    // interval:    seconds between this leak's last two drops (for the ripple rings).
    struct DripSite
    {
        math::Vec3 source{0.0f, 0.0f, 0.0f};
        math::Vec3 floor{0.0f, 0.0f, 0.0f};
        float fallSeconds{0.8f};
        float untilDrop{0.0f};
        float untilLanding{-1.0f};
        float dropAge{10.0f};
        float interval{1.6f};
    };

    // Water dripping from leaks in the ceiling (level markers named "Fx_Drip_*", placed just under
    // the ceiling): a drop forms, falls, splashes into a small puddle on the floor below and sends
    // ripple rings across it (render::DripPuddle), with a quiet plink. Presentation only, World
    // time (bullet-time slows the drops).
    // tuning:   settings.
    // sites:    the leaks of `scene`.
    // scene:    the scene they were found in.
    // drop, splash: particle effects, once created.
    // sounds:   drop sounds made at start (synthesised).
    // isTried:  creating the effects and sounds was attempted.
    // random:   presentation-only random state.
    struct Drips
    {
        DripTuning tuning{};
        std::vector<DripSite> sites{};
        const lockstep::assets::Scene* scene{nullptr};
        lockstep::render::ParticleEffectId drop{};
        lockstep::render::ParticleEffectId splash{};
        std::vector<lockstep::audio::SoundHandle> sounds{};
        bool isTried{false};
        uint32_t random{0x1B873593u};
    };

    // Finds the leaks when the level changes, lets drops fall, splashes them and submits the
    // puddles. Call before presentEffects() (which simulates and submits the particles).
    // drips:        the drips.
    // state:        the game state (level, particles, settings).
    // engine:       the engine (renderer, physics, audio).
    // eye:          the camera position.
    // worldSeconds: World seconds since the previous frame.
    void presentDrips(Drips& drips, lockstep::app::GameState& state, lockstep::app::EngineContext& engine,
        const math::Vec3& eye, float worldSeconds);

#if LOCKSTEP_DEV_TOOLS
    // Draws the "Drips" section of the Game dev panel.
    // drips: the drips.
    void dripsDevSection(Drips& drips);
#endif
} //namespace game

#endif //LOCKSTEP_GAME_EFFECTS_DRIPS_HPP
