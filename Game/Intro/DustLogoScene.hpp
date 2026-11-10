#ifndef LOCKSTEP_GAME_INTRO_DUST_LOGO_SCENE_HPP
#define LOCKSTEP_GAME_INTRO_DUST_LOGO_SCENE_HPP

#include "Gore/GoreRandom.hpp"

#include <Lockstep/Core/Math.hpp>
#include <Lockstep/Core/Result.hpp>
#include <Lockstep/Render/Particles.hpp>
#include <Lockstep/Render/Resources.hpp>

#include <vector>

namespace lockstep::render
{
    class Renderer;
} //namespace lockstep::render

namespace game
{
    namespace math = lockstep::math;

    // When things happen on the "Made with Vela" card, in seconds from its start.
    // gatherStart: dust begins to gather into the wordmark.
    // holdStart:   the wordmark is complete.
    // gust:        the gust blows it apart.
    // length:      the card ends (the beam has faded out).
    struct DustLogoTiming
    {
        float gatherStart{0.7f};
        float holdStart{4.1f};
        float gust{6.2f};
        float length{9.2f};
    };

    // One dust mote of the intro. Presentation only.
    // start:      where it floats at the card's start.
    // drift:      its slow drift velocity in m/s.
    // phase:      phase of its wander.
    // target:     its place in the wordmark, if it joins it.
    // swirl:      sideways offset of its path while it gathers.
    // delay:      seconds after DustLogoTiming::gatherStart before it starts gathering.
    // gatherSeconds: how long its gathering takes.
    // size:       size factor.
    // brightness: opacity factor.
    // joins:      it gathers into the wordmark (others only float).
    // velocity:   its velocity after the gust reached it, in m/s.
    // gustDelay:  seconds after DustLogoTiming::gust until the gust reaches it.
    // isBlown:    the gust has reached it.
    // position:   where it is now.
    struct DustMote
    {
        math::Vec3 start{0.0f, 0.0f, 0.0f};
        math::Vec3 drift{0.0f, 0.0f, 0.0f};
        math::Vec3 phase{0.0f, 0.0f, 0.0f};
        math::Vec3 target{0.0f, 0.0f, 0.0f};
        math::Vec3 swirl{0.0f, 0.0f, 0.0f};
        float delay{0.0f};
        float gatherSeconds{1.0f};
        float size{1.0f};
        float brightness{1.0f};
        bool joins{false};
        math::Vec3 velocity{0.0f, 0.0f, 0.0f};
        float gustDelay{0.0f};
        bool isBlown{false};
        math::Vec3 position{0.0f, 0.0f, 0.0f};
    };

    // The real-time "Made with Vela" scene: a dark room, one spot light beam through volumetric
    // fog, and dust motes that drift in, catch the light, gather into the VELA wordmark and are
    // blown apart by a gust. The motes are moved here and drawn through a ParticleSystem used as
    // an immediate-mode sprite list (cleared and refilled every frame).
    // particles: the motes' particle system.
    // effect:    the mote sprite effect.
    // sprite:    the soft round mote texture.
    // floorMesh, floorMaterial, floorTexture: the floor the beam lands on.
    // motes:     every mote.
    // random:    turbulence noise.
    struct DustLogoScene
    {
        lockstep::render::ParticleSystem particles;
        lockstep::render::ParticleEffectId effect{};
        lockstep::render::TextureHandle sprite{};
        lockstep::render::MeshHandle floorMesh{};
        lockstep::render::MaterialHandle floorMaterial{};
        lockstep::render::TextureHandle floorTexture{};
        std::vector<DustMote> motes;
        GoreRandom random{};
    };

    // Creates the scene's GPU resources and places the motes.
    // renderer: the renderer.
    // Returns:  the scene, or the renderer's error.
    [[nodiscard]] lockstep::Result<DustLogoScene> createDustLogoScene(lockstep::render::Renderer& renderer);

    // Releases the scene's GPU resources.
    // scene:    the scene.
    // renderer: the renderer that created them.
    void destroyDustLogoScene(DustLogoScene& scene, lockstep::render::Renderer& renderer);

    // Moves the motes and submits the camera, light, floor and motes for this frame.
    // scene:    the scene.
    // renderer: the renderer.
    // seconds:  time since the card started.
    // delta:    seconds since the previous frame.
    // light:    brightness of the beam, 0 (off) to 1.
    void presentDustLogoScene(DustLogoScene& scene, lockstep::render::Renderer& renderer, float seconds, float delta,
        float light);
} //namespace game

#endif //LOCKSTEP_GAME_INTRO_DUST_LOGO_SCENE_HPP
