#include "AudioMix.hpp"

#include <algorithm>
#include <cmath>

namespace lockstep::audio
{
    namespace
    {
        constexpr float k_openCutoffHz{22000.0f};
        constexpr float k_minCutoffHz{20.0f};
        constexpr float k_openScale{0.999f};
        constexpr float k_teleportSpeed{40.0f};
    } //namespace

    float smoothTowards(float current, float target, float seconds, float timeConstantSeconds)
    {
        if (timeConstantSeconds <= 0.0f || seconds <= 0.0f)
        {
            return timeConstantSeconds <= 0.0f ? target : current;
        }

        const float blend = 1.0f - std::exp(-seconds / timeConstantSeconds);

        return current + (target - current) * blend;
    }

    BusMix busMix(float volume, float scale, const TimeScaleEffect& effect)
    {
        BusMix mix{};
        mix.volume = volume;
        mix.isPaused = scale < k_minPitch;
        mix.pitch = effect.followsPitch ? std::max(scale, k_minPitch) : 1.0f;

        if (effect.lowPass && scale < k_openScale)
        {
            const float closedCutoff = std::clamp(effect.lowPassCutoffHz, k_minCutoffHz, k_openCutoffHz);
            const float openness = std::clamp(scale, 0.0f, 1.0f);
            mix.lowPassCutoffHz = closedCutoff * std::pow(k_openCutoffHz / closedCutoff, openness);
        }

        return mix;
    }

    math::Vec3 listenerVelocity(const math::Vec3& previous, const math::Vec3& current, float seconds)
    {
        if (seconds <= 0.0f)
        {
            return math::Vec3{0.0f, 0.0f, 0.0f};
        }

        const math::Vec3 velocity = (current - previous) * (1.0f / seconds);

        if (math::lengthSquared(velocity) > k_teleportSpeed * k_teleportSpeed)
        {
            return math::Vec3{0.0f, 0.0f, 0.0f};
        }

        return velocity;
    }
} //namespace lockstep::audio
