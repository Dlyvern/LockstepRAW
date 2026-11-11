#include "Weather/Wind.hpp"

#include <algorithm>
#include <cmath>

namespace game
{
    namespace
    {
        namespace math = lockstep::math;

        constexpr float k_minGustSeconds{1.5f};
        constexpr float k_gustSecondsRange{4.5f};
        constexpr float k_gustEaseSeconds{1.2f};
        constexpr float k_gustRise{1.2f};
        constexpr float k_gustDrop{0.35f};
        constexpr double k_headingPeriodSeconds{90.0};
        constexpr double k_clockWrapSeconds{3600.0};

        float unit(Wind& wind)
        {
            wind.random ^= wind.random << 13;
            wind.random ^= wind.random >> 17;
            wind.random ^= wind.random << 5;
            return static_cast<float>(wind.random >> 8) * (1.0f / 16777216.0f);
        }
    } //namespace

    void updateWind(Wind& wind, const WindTuning& tuning, float rain, float worldSeconds)
    {
        const float seconds = std::max(worldSeconds, 0.0f);
        wind.gustSeconds -= seconds;

        if (wind.gustSeconds <= 0.0f)
        {
            const float pick = unit(wind);
            wind.gustTarget = pick * pick;
            wind.gustSeconds = k_minGustSeconds + unit(wind) * k_gustSecondsRange;
        }

        wind.gust += (wind.gustTarget - wind.gust) * std::min(seconds / k_gustEaseSeconds, 1.0f);
        wind.headingSeconds = std::fmod(wind.headingSeconds + seconds, k_clockWrapSeconds * 10.0);

        const double phase = wind.headingSeconds * 2.0 * 3.14159265358979 / k_headingPeriodSeconds;
        const float wanderDegrees = tuning.wander
            * static_cast<float>(0.7 * std::sin(phase) + 0.3 * std::sin(phase * 2.7 + 1.3));
        const float heading = math::radians(tuning.heading + wanderDegrees);
        wind.direction = math::Vec2{std::sin(heading), -std::cos(heading)};

        const float mean = tuning.enabled
            ? tuning.clearSpeed + (tuning.stormSpeed - tuning.clearSpeed) * std::clamp(rain, 0.0f, 1.0f) : 0.0f;
        const float gustiness = std::clamp(tuning.gustiness, 0.0f, 1.0f);
        wind.speed = std::max(mean * (1.0f + gustiness * (wind.gust * k_gustRise - k_gustDrop)), 0.0f);

        wind.swaySeconds = std::fmod(wind.swaySeconds + seconds, k_clockWrapSeconds);
        const float drift = wind.speed * std::max(tuning.cloudSpeed, 0.0f) * seconds;
        wind.cloudOffset = wind.cloudOffset - wind.direction * drift;
    }

    math::Vec3 windVelocity(const Wind& wind)
    {
        return math::Vec3{wind.direction.x, 0.0f, wind.direction.y} * wind.speed;
    }

    void applyWind(const Wind& wind, lockstep::render::RenderSettings& settings)
    {
        settings.wind.direction = wind.direction;
        settings.wind.speed = wind.speed;
        settings.wind.time = static_cast<float>(wind.swaySeconds);
        settings.clouds.offset = wind.cloudOffset;
    }
} //namespace game
