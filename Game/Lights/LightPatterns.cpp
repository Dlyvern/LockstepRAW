#include "Lights/LightPatterns.hpp"

#include <algorithm>
#include <cmath>

namespace game
{
    namespace
    {
        constexpr std::string_view k_flickerHardTag{"_FlickerHard"};
        constexpr std::string_view k_flickerTag{"_Flicker"};
        constexpr std::string_view k_sirenRedTag{"_SirenRed"};
        constexpr std::string_view k_sirenBlueTag{"_SirenBlue"};
        constexpr std::string_view k_pulseTag{"_Pulse"};

        constexpr math::Vec3 k_sirenRed{1.0f, 0.04f, 0.02f};
        constexpr math::Vec3 k_sirenBlue{0.04f, 0.16f, 1.0f};

        constexpr double k_minWindow{0.2};
        constexpr double k_minSlot{0.005};
        constexpr float k_dyingSeconds{0.25f};
        constexpr float k_restrikeSeconds{0.4f};
        constexpr float k_minOffSeconds{0.6f};
        constexpr float k_extraOffSeconds{1.6f};
        constexpr float k_minBurstSeconds{0.15f};
        constexpr float k_extraBurstSeconds{0.75f};
        constexpr float k_stutterOnChance{0.55f};
        constexpr float k_restrikeOnChance{0.4f};
        constexpr float k_dyingOnChance{0.5f};
        constexpr float k_hardBurstChance{0.35f};
        constexpr float k_hardOnChance{0.12f};
        constexpr float k_hardSparkChance{0.012f};
        constexpr float k_hardStutterOnChance{0.6f};
        constexpr float k_hardMinOnSeconds{0.8f};
        constexpr float k_hardExtraOnSeconds{1.7f};
        constexpr float k_hardMinBurstSeconds{0.2f};
        constexpr float k_hardExtraBurstSeconds{1.0f};
        constexpr float k_hardEdgeSeconds{0.2f};
        constexpr float k_twoPi{6.28318531f};

        uint32_t mix(uint32_t value, uint32_t salt)
        {
            uint32_t hash = value ^ (salt * 0x9E3779B9u);
            hash ^= hash >> 16;
            hash *= 0x85EBCA6Bu;
            hash ^= hash >> 13;
            hash *= 0xC2B2AE35u;
            hash ^= hash >> 16;
            return hash;
        }

        float unit(uint32_t hash)
        {
            return static_cast<float>(hash >> 8) / 16777216.0f;
        }

        struct Episode
        {
            uint32_t hash{0};
            float roll{0.0f};
            float seconds{0.0f};
            float length{0.0f};
        };

        Episode flickerEpisode(uint32_t seed, double seconds, const LightPatternTuning& tuning)
        {
            const double window = std::max(static_cast<double>(tuning.flickerWindow), k_minWindow);
            const double shifted = seconds + static_cast<double>(unit(mix(seed, 11u))) * window;
            const double index = std::floor(shifted / window);
            const uint32_t hash = mix(seed, static_cast<uint32_t>(static_cast<int64_t>(index)));

            return Episode{.hash = hash,
                .roll = unit(hash),
                .seconds = static_cast<float>(shifted - index * window),
                .length = static_cast<float>(window)};
        }

        bool stutterIsOn(uint32_t seed, double seconds, const LightPatternTuning& tuning, float onChance)
        {
            const double slot = std::max(static_cast<double>(tuning.flickerSlot), k_minSlot);
            const auto slotIndex = static_cast<int64_t>(std::floor(seconds / slot));
            return unit(mix(seed ^ 0x5BD1E995u, static_cast<uint32_t>(slotIndex))) < onChance;
        }

        bool slotChance(uint32_t seed, double seconds, const LightPatternTuning& tuning, float chance)
        {
            return stutterIsOn(seed ^ 0x27D4EB2Fu, seconds, tuning, chance);
        }

        float spanStart(const Episode& episode, float length, float margin)
        {
            const float room = std::max(episode.length - length - margin, 0.0f);
            return unit(mix(episode.hash, 1u)) * room;
        }

        float spanLength(const Episode& episode, float minimum, float extra)
        {
            return std::min(minimum + unit(mix(episode.hash, 2u)) * extra, episode.length);
        }

        bool flickerIsOn(uint32_t seed, double seconds, const LightPatternTuning& tuning)
        {
            const Episode episode = flickerEpisode(seed, seconds, tuning);
            const float time = episode.seconds;

            if (episode.roll < tuning.flickerOffChance)
            {
                const float length = spanLength(episode, k_minOffSeconds, k_extraOffSeconds);
                const float start = k_dyingSeconds + spanStart(episode, length, k_dyingSeconds + k_restrikeSeconds);
                const float end = start + length;

                if (time >= start - k_dyingSeconds && time < start)
                {
                    return stutterIsOn(seed, seconds, tuning, k_dyingOnChance);
                }

                if (time >= start && time < end)
                {
                    return false;
                }

                if (time >= end && time < end + k_restrikeSeconds)
                {
                    return stutterIsOn(seed, seconds, tuning, k_restrikeOnChance);
                }

                return true;
            }

            if (episode.roll < tuning.flickerOffChance + tuning.flickerBurstChance)
            {
                const float length = spanLength(episode, k_minBurstSeconds, k_extraBurstSeconds);
                const float start = spanStart(episode, length, 0.0f);

                if (time >= start && time < start + length)
                {
                    return stutterIsOn(seed, seconds, tuning, k_stutterOnChance);
                }

                return true;
            }

            return !slotChance(seed, seconds, tuning, tuning.flickerDropout);
        }

        bool hardFlickerIsOn(uint32_t seed, double seconds, const LightPatternTuning& tuning)
        {
            const Episode episode = flickerEpisode(seed, seconds, tuning);
            const float time = episode.seconds;

            if (episode.roll < k_hardOnChance)
            {
                const float length = spanLength(episode, k_hardMinOnSeconds, k_hardExtraOnSeconds);
                const float start = spanStart(episode, length, 0.0f);
                const float end = start + length;

                if (time < start || time >= end)
                {
                    return slotChance(seed, seconds, tuning, k_hardSparkChance);
                }

                if (time < start + k_hardEdgeSeconds || time >= end - k_hardEdgeSeconds)
                {
                    return stutterIsOn(seed, seconds, tuning, k_hardStutterOnChance);
                }

                return true;
            }

            if (episode.roll < k_hardOnChance + k_hardBurstChance)
            {
                const float length = spanLength(episode, k_hardMinBurstSeconds, k_hardExtraBurstSeconds);
                const float start = spanStart(episode, length, 0.0f);

                if (time >= start && time < start + length)
                {
                    return stutterIsOn(seed, seconds, tuning, k_hardStutterOnChance);
                }
            }

            return slotChance(seed, seconds, tuning, k_hardSparkChance);
        }

        float sirenLevel(LightPattern pattern, uint32_t seed, double seconds, const LightPatternTuning& tuning)
        {
            const float rate = 1.0f + (unit(mix(seed, 3u)) * 2.0f - 1.0f) * tuning.sirenRateJitter;
            const double cycle = std::max(static_cast<double>(tuning.sirenCycle), k_minWindow) / rate;
            const double phase = seconds / cycle + static_cast<double>(unit(mix(seed, 4u)));
            const float inCycle = static_cast<float>((phase - std::floor(phase)) * cycle);
            const float half = static_cast<float>(cycle) * 0.5f;
            const float local = pattern == LightPattern::SirenRed ? inCycle : inCycle - half;

            if (local < 0.0f || local >= half)
            {
                return 0.0f;
            }

            const bool firstFlash = local < tuning.sirenFlash;
            const float secondStart = tuning.sirenFlash + tuning.sirenGap;
            const bool secondFlash = local >= secondStart && local < secondStart + tuning.sirenFlash;

            return firstFlash || secondFlash ? 1.0f : 0.0f;
        }

        float pulseLevel(uint32_t seed, double seconds, const LightPatternTuning& tuning)
        {
            const double period = std::max(static_cast<double>(tuning.pulsePeriod), k_minWindow);
            const double phase = seconds / period + static_cast<double>(unit(mix(seed, 5u)));
            const float wave = 0.5f - 0.5f * std::cos(k_twoPi * static_cast<float>(phase - std::floor(phase)));

            return tuning.pulseLow + (1.0f - tuning.pulseLow) * wave;
        }
    } //namespace

    LightPattern lightPatternFromName(std::string_view name)
    {
        if (name.find(k_flickerHardTag) != std::string_view::npos)
        {
            return LightPattern::FlickerHard;
        }

        if (name.find(k_flickerTag) != std::string_view::npos)
        {
            return LightPattern::Flicker;
        }

        if (name.find(k_sirenRedTag) != std::string_view::npos)
        {
            return LightPattern::SirenRed;
        }

        if (name.find(k_sirenBlueTag) != std::string_view::npos)
        {
            return LightPattern::SirenBlue;
        }

        if (name.find(k_pulseTag) != std::string_view::npos)
        {
            return LightPattern::Pulse;
        }

        return LightPattern::None;
    }

    const char* lightPatternName(LightPattern pattern)
    {
        switch (pattern)
        {
        case LightPattern::None:
            return "None";
        case LightPattern::Flicker:
            return "Flicker";
        case LightPattern::FlickerHard:
            return "FlickerHard";
        case LightPattern::SirenRed:
            return "SirenRed";
        case LightPattern::SirenBlue:
            return "SirenBlue";
        case LightPattern::Pulse:
            return "Pulse";
        }

        return "None";
    }

    uint32_t lightSeed(std::string_view name, uint32_t index)
    {
        uint32_t hash = 2166136261u;

        for (const char character : name)
        {
            hash ^= static_cast<uint8_t>(character);
            hash *= 16777619u;
        }

        return mix(hash, index);
    }

    float lightPatternLevel(LightPattern pattern, uint32_t seed, double seconds, const LightPatternTuning& tuning)
    {
        switch (pattern)
        {
        case LightPattern::None:
            return 1.0f;
        case LightPattern::Flicker:
            return flickerIsOn(seed, seconds, tuning) ? 1.0f : tuning.flickerOffLevel;
        case LightPattern::FlickerHard:
            return hardFlickerIsOn(seed, seconds, tuning) ? 1.0f : tuning.flickerOffLevel;
        case LightPattern::SirenRed:
        case LightPattern::SirenBlue:
            return sirenLevel(pattern, seed, seconds, tuning);
        case LightPattern::Pulse:
            return pulseLevel(seed, seconds, tuning);
        }

        return 1.0f;
    }

    std::optional<math::Vec3> lightPatternColor(LightPattern pattern)
    {
        if (pattern == LightPattern::SirenRed)
        {
            return k_sirenRed;
        }

        if (pattern == LightPattern::SirenBlue)
        {
            return k_sirenBlue;
        }

        return std::nullopt;
    }
} //namespace game
