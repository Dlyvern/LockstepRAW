#ifndef LOCKSTEP_AUDIO_AUDIO_MIX_HPP
#define LOCKSTEP_AUDIO_AUDIO_MIX_HPP

#include "AudioBackend.hpp"

#include "Lockstep/Audio/AudioTypes.hpp"
#include "Lockstep/Core/Math.hpp"

namespace lockstep::audio
{
    // Lowest playback rate a voice or bus gets; below it a bus counts as paused.
    inline constexpr float k_minPitch{0.02f};

    // Highest bus volume and time scale.
    inline constexpr float k_maxBusValue{4.0f};

    // Moves a value towards a target with exponential smoothing.
    // current:             the value now.
    // target:              where it is heading.
    // seconds:             time since the last call.
    // timeConstantSeconds: time to cover ~63% of the distance; 0 or less jumps to the target.
    // Returns:             the new value.
    [[nodiscard]] float smoothTowards(float current, float target, float seconds, float timeConstantSeconds);

    // Computes what a bus applies.
    // volume:  the bus volume.
    // scale:   the bus' smoothed time scale.
    // effect:  how the bus reacts to its time scale.
    // Returns: volume, pitch, low-pass cutoff (0 when open) and pause state.
    [[nodiscard]] BusMix busMix(float volume, float scale, const TimeScaleEffect& effect);

    // Derives the listener velocity from its movement between two frames. A jump faster than a
    // teleport threshold (respawn, camera switch) counts as standing still, so it causes no
    // Doppler sweep.
    // previous: the listener position last frame.
    // current:  the listener position now.
    // seconds:  time between the two.
    // Returns:  velocity in m/s.
    [[nodiscard]] math::Vec3 listenerVelocity(const math::Vec3& previous, const math::Vec3& current, float seconds);
} //namespace lockstep::audio

#endif //LOCKSTEP_AUDIO_AUDIO_MIX_HPP
