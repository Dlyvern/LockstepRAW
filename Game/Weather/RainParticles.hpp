#ifndef LOCKSTEP_GAME_WEATHER_RAIN_PARTICLES_HPP
#define LOCKSTEP_GAME_WEATHER_RAIN_PARTICLES_HPP

#include "Audio/CombatAudio.hpp"
#include "Effects/EffectLibrary.hpp"

#include <Lockstep/Core/Math.hpp>
#include <Lockstep/Render/Particles.hpp>

#include <cstdint>
#include <optional>
#include <span>

namespace lockstep::physics
{
    class PhysicsWorld;
} //namespace lockstep::physics

namespace game
{
    // The rain's particles: streaks from an emitter above the camera (they die on entering an
    // interior box, so rain stops at roofs) and droplet splashes on the ground near the camera.
    // streaks:       the streak emitter, once created (reset when the particle system is cleared).
    // streakStretch: stretch last written to the streak effect (bullet-time shortens the streaks
    //                to falling drops).
    // splashCarry:   fractional splashes carried to the next frame.
    // lastEye:       the camera position last frame (the emitter leads a moving camera).
    // random:        presentation-only random state.
    struct RainParticles
    {
        std::optional<lockstep::render::ParticleEmitterId> streaks{};
        float streakStretch{-1.0f};
        float splashCarry{0.0f};
        std::optional<lockstep::math::Vec3> lastEye{};
        uint32_t random{0x1B873593u};
    };

    // What the rain's particles read each frame.
    // streaks:      streak rate scale (rain amount times density; 0 for none).
    // splashes:     splashes per second (0 for none).
    // physics:      the physics world (where splashes land).
    // interiors:    the level's interior boxes (no splashes inside).
    // eye:          the camera position.
    // worldSeconds: World time since the previous frame.
    // frameSeconds: real time since the previous frame.
    // worldScale:   the World time scale (bullet-time shortens the streaks).
    struct RainFrame
    {
        float streaks{0.0f};
        float splashes{0.0f};
        const lockstep::physics::PhysicsWorld* physics{nullptr};
        std::span<const InteriorBox> interiors{};
        lockstep::math::Vec3 eye{0.0f, 0.0f, 0.0f};
        float worldSeconds{0.0f};
        float frameSeconds{0.0f};
        float worldScale{1.0f};
    };

    // Moves the streak emitter with the camera, sets its rate and the streak length, and spawns
    // this frame's splashes. Call before the particle system's update().
    // rain:      the rain particle state.
    // particles: the particle system.
    // ids:       the game's effects (rainStreaks, rainSplash).
    // frame:     this frame's rain.
    void updateRainParticles(RainParticles& rain, lockstep::render::ParticleSystem& particles, const EffectIds& ids,
        const RainFrame& frame);
} //namespace game

#endif //LOCKSTEP_GAME_WEATHER_RAIN_PARTICLES_HPP
