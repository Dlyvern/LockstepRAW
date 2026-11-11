#include "Weather/Lightning.hpp"

#include <algorithm>
#include <cmath>

namespace game
{
    namespace
    {
        namespace math = lockstep::math;

        constexpr float k_speedOfSound{343.0f};
        constexpr float k_strokeAttack{0.012f};
        constexpr float k_strokeDecay{0.07f};
        constexpr float k_strikeSeconds{1.2f};
        constexpr float k_nearDistance{1800.0f};
        constexpr float k_minStrikeBrightness{0.25f};
        constexpr float k_maxStrikeBrightness{1.4f};
        constexpr float k_minLoudness{0.15f};
        constexpr float k_maxLoudness{1.2f};
        constexpr float k_minStrokeGap{0.05f};
        constexpr float k_strokeGapRange{0.15f};
        constexpr float k_boltShowsAbove{0.15f};

        float unit(Lightning& lightning)
        {
            lightning.random ^= lightning.random << 13;
            lightning.random ^= lightning.random >> 17;
            lightning.random ^= lightning.random << 5;
            return static_cast<float>(lightning.random >> 8) * (1.0f / 16777216.0f);
        }

        float range(Lightning& lightning, float low, float high)
        {
            return low + (high - low) * unit(lightning);
        }

        float strokeBrightness(const Lightning& lightning)
        {
            float flash = 0.0f;

            for (uint32_t stroke = 0; stroke < lightning.strokes; ++stroke)
            {
                const float since = lightning.age - lightning.strokeTimes[stroke];

                if (since < 0.0f)
                {
                    continue;
                }

                const float rise = std::min(since / k_strokeAttack, 1.0f);
                flash += lightning.strokePeaks[stroke] * rise * std::exp(-std::max(since - k_strokeAttack, 0.0f)
                    / k_strokeDecay);
            }

            return flash;
        }

        float nextInterval(Lightning& lightning, const LightningTuning& tuning, float rain)
        {
            const float lighter = 1.0f / std::max(rain, 0.1f);
            return range(lightning, tuning.minSeconds, std::max(tuning.maxSeconds, tuning.minSeconds)) * lighter;
        }
    } //namespace

    void strikeLightning(Lightning& lightning, const LightningTuning& tuning)
    {
        const float cloudBase = lightning.cloudBase;
        const float distance = range(lightning, tuning.minDistance, std::max(tuning.maxDistance, tuning.minDistance));
        const float azimuth = range(lightning, 0.0f, 2.0f * math::k_pi);
        lightning.distance = distance;
        lightning.direction = math::normalize(math::Vec3{std::cos(azimuth) * distance, std::max(cloudBase, 200.0f),
            std::sin(azimuth) * distance});
        lightning.boltSeed = range(lightning, 0.0f, 100.0f);
        lightning.age = 0.0f;
        lightning.strokes = 2 + static_cast<uint32_t>(unit(lightning) * 3.0f);
        lightning.strokes = std::min<uint32_t>(lightning.strokes, static_cast<uint32_t>(lightning.strokeTimes.size()));

        const float brightness = std::clamp(k_nearDistance / distance, k_minStrikeBrightness, k_maxStrikeBrightness);
        float time = 0.0f;

        for (uint32_t stroke = 0; stroke < lightning.strokes; ++stroke)
        {
            lightning.strokeTimes[stroke] = time;
            lightning.strokePeaks[stroke] = brightness * (stroke == 0 ? 1.0f : range(lightning, 0.35f, 0.9f));
            time += k_minStrokeGap + unit(lightning) * k_strokeGapRange;
        }

        const ThunderCue cue{.direction = lightning.direction,
            .distance = distance,
            .loudness = std::clamp(k_nearDistance / distance, k_minLoudness, k_maxLoudness)};
        lightning.pending.emplace_back(distance / k_speedOfSound, cue);
    }

    void updateLightning(Lightning& lightning, const LightningTuning& tuning, float rain, float cloudBase,
        float worldSeconds)
    {
        const float seconds = std::max(worldSeconds, 0.0f);
        lightning.cloudBase = cloudBase;
        lightning.due.clear();

        for (auto& [remaining, cue] : lightning.pending)
        {
            remaining -= seconds;

            if (remaining <= 0.0f)
            {
                lightning.due.push_back(cue);
            }
        }

        std::erase_if(lightning.pending, [](const auto& entry) { return entry.first <= 0.0f; });

        if (lightning.age >= 0.0f)
        {
            lightning.age += seconds;
            lightning.flash = strokeBrightness(lightning);

            if (lightning.age > k_strikeSeconds)
            {
                lightning.age = -1.0f;
                lightning.flash = 0.0f;
            }
        }

        const bool canStrike = tuning.enabled && rain >= tuning.minRain;

        if (!canStrike)
        {
            lightning.untilStrike = std::max(lightning.untilStrike, tuning.minSeconds * 0.5f);
            return;
        }

        lightning.untilStrike -= seconds;

        if (lightning.untilStrike <= 0.0f && lightning.age < 0.0f)
        {
            strikeLightning(lightning, tuning);
            lightning.untilStrike = nextInterval(lightning, tuning, rain);
        }
    }

    void applyLightning(const Lightning& lightning, const LightningTuning& tuning,
        lockstep::render::RenderSettings& settings)
    {
        lockstep::render::LightningSettings& flash = settings.lightning;
        flash.flash = lightning.flash * std::max(tuning.brightness, 0.0f);
        flash.direction = lightning.direction;
        flash.bolt = tuning.bolts && lightning.flash > k_boltShowsAbove ? 1.0f : 0.0f;
        flash.boltSeed = lightning.boltSeed;
    }
} //namespace game
