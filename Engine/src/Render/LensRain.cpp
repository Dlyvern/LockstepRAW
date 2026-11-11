#include "Lockstep/Render/LensRain.hpp"

#include "Lockstep/Render/Renderer.hpp"

#include <algorithm>
#include <cmath>

namespace lockstep::render
{
    namespace
    {
        constexpr float k_minRadius{0.004f};
        constexpr float k_radiusRange{0.017f};
        constexpr float k_runRadius{0.011f};
        constexpr float k_runSpeedPerRadius{30.0f};
        constexpr float k_maxRunSpeed{0.45f};
        constexpr float k_runAcceleration{2.5f};
        constexpr float k_stickChancePerSecond{1.6f};
        constexpr float k_minStickSeconds{0.04f};
        constexpr float k_stickSecondsRange{0.35f};
        constexpr float k_trailSpacing{0.014f};
        constexpr float k_trailRadiusShare{0.32f};
        constexpr float k_evaporationRadiusPerSecond{0.00045f};
        constexpr float k_vanishRadius{0.0012f};
        constexpr float k_fadeInSeconds{0.08f};
        constexpr float k_mergeReach{0.85f};
        constexpr float k_maxStretch{1.9f};
        constexpr float k_stretchPerSpeed{4.0f};
        constexpr float k_bottomEdge{1.08f};
        constexpr float k_wanderPerSecond{0.02f};
        constexpr uint32_t k_maxLandingsPerUpdate{24};
        constexpr float k_wipeBand{0.08f};

        float radiusOf(float mass)
        {
            return std::cbrt(std::max(mass, 0.0f));
        }

        float massOf(float radius)
        {
            return radius * radius * radius;
        }
    } //namespace

    float LensRain::random()
    {
        m_random ^= m_random << 13;
        m_random ^= m_random >> 17;
        m_random ^= m_random << 5;
        return static_cast<float>(m_random >> 8) * (1.0f / 16777216.0f);
    }

    void LensRain::land(const LensRainFrame& frame)
    {
        m_landCarry += std::max(frame.landRate, 0.0f) * frame.seconds;
        const auto count = std::min(static_cast<uint32_t>(std::floor(m_landCarry)), k_maxLandingsPerUpdate);
        m_landCarry -= std::floor(m_landCarry);

        for (uint32_t index = 0; index < count && m_drops.size() < k_maxLensDroplets; ++index)
        {
            const float size = random();
            const float radius = k_minRadius + k_radiusRange * size * size * size;
            m_drops.push_back(Drop{.position = math::Vec2{random(), random() * 1.05f - 0.05f},
                .mass = massOf(radius),
                .speed = 0.0f,
                .stuck = random() * k_stickSecondsRange,
                .trail = 0.0f,
                .opacity = 0.0f});
        }
    }

    void LensRain::move(Drop& drop, const LensRainFrame& frame)
    {
        const float seconds = frame.seconds;
        const float radius = radiusOf(drop.mass);
        drop.opacity = std::min(drop.opacity + seconds / k_fadeInSeconds, 1.0f);

        const float shrunk = std::max(radius - k_evaporationRadiusPerSecond * std::max(frame.evaporation, 0.0f) * seconds,
            0.0f);
        drop.mass = massOf(shrunk);

        const float gravity = std::clamp(frame.gravity, 0.0f, 1.0f);
        const float targetSpeed = std::min((shrunk - k_runRadius) * k_runSpeedPerRadius * gravity, k_maxRunSpeed);

        if (targetSpeed <= 0.0f)
        {
            drop.speed = 0.0f;
            return;
        }

        if (drop.stuck > 0.0f)
        {
            drop.stuck -= seconds;
            drop.speed = 0.0f;
            return;
        }

        if (random() < k_stickChancePerSecond * seconds)
        {
            drop.stuck = k_minStickSeconds + random() * k_stickSecondsRange;
        }

        drop.speed = std::min(drop.speed + k_runAcceleration * seconds, targetSpeed);
        const float step = drop.speed * seconds;
        const float wander = (random() - 0.5f) * k_wanderPerSecond * seconds * 10.0f;
        drop.position.y += step + frame.drift.y * seconds;
        drop.position.x += wander / std::max(frame.aspectRatio, 0.1f) + frame.drift.x * seconds;
        drop.trail += step;

        if (drop.trail < k_trailSpacing || m_drops.size() >= k_maxLensDroplets)
        {
            return;
        }

        drop.trail = 0.0f;
        const float beadRadius = shrunk * k_trailRadiusShare * (0.6f + 0.8f * random());
        const float beadMass = std::min(massOf(beadRadius), drop.mass * 0.25f);
        drop.mass -= beadMass;
        m_drops.push_back(Drop{.position = math::Vec2{drop.position.x, drop.position.y - radiusOf(drop.mass) * 1.2f},
            .mass = beadMass,
            .speed = 0.0f,
            .stuck = 1.0f,
            .trail = 0.0f,
            .opacity = 1.0f});
    }

    void LensRain::merge(float aspectRatio)
    {
        const std::size_t count = m_drops.size();

        for (std::size_t first = 0; first < count; ++first)
        {
            Drop& a = m_drops[first];

            if (a.mass <= 0.0f)
            {
                continue;
            }

            for (std::size_t second = first + 1; second < count; ++second)
            {
                Drop& b = m_drops[second];

                if (b.mass <= 0.0f)
                {
                    continue;
                }

                const float dx = (a.position.x - b.position.x) * aspectRatio;
                const float dy = a.position.y - b.position.y;
                const float reach = (radiusOf(a.mass) + radiusOf(b.mass)) * k_mergeReach;

                if (dx * dx + dy * dy > reach * reach)
                {
                    continue;
                }

                Drop& keep = a.mass >= b.mass ? a : b;
                Drop& gone = a.mass >= b.mass ? b : a;
                const float total = keep.mass + gone.mass;
                keep.position = (keep.position * keep.mass + gone.position * gone.mass) * (1.0f / total);
                keep.mass = total;
                keep.speed = std::max(keep.speed, gone.speed);
                keep.opacity = std::max(keep.opacity, gone.opacity);
                gone.mass = 0.0f;

                if (a.mass <= 0.0f)
                {
                    break;
                }
            }
        }
    }

    void LensRain::sweep(float seconds)
    {
        if (m_wipeDuration <= 0.0f)
        {
            return;
        }

        m_wipeSeconds += seconds;
        const float band = m_wipeSeconds / m_wipeDuration * (1.0f + k_wipeBand);

        for (Drop& drop : m_drops)
        {
            if (drop.position.y < band)
            {
                drop.mass = 0.0f;
            }
        }

        if (m_wipeSeconds >= m_wipeDuration)
        {
            m_wipeDuration = 0.0f;
            m_wipeSeconds = 0.0f;
        }
    }

    void LensRain::update(const LensRainFrame& frame)
    {
        if (frame.seconds <= 0.0f)
        {
            return;
        }

        m_drops.reserve(k_maxLensDroplets);
        land(frame);

        for (std::size_t index = 0; index < m_drops.size(); ++index)
        {
            move(m_drops[index], frame);
        }

        merge(std::max(frame.aspectRatio, 0.1f));
        sweep(frame.seconds);

        std::erase_if(m_drops, [](const Drop& drop)
        {
            return radiusOf(drop.mass) < k_vanishRadius || drop.position.y - radiusOf(drop.mass) > k_bottomEdge;
        });

        m_view.clear();

        for (const Drop& drop : m_drops)
        {
            const float radius = radiusOf(drop.mass);
            const float fade = std::clamp((radius - k_vanishRadius) / k_vanishRadius, 0.0f, 1.0f);
            m_view.push_back(LensDroplet{.position = drop.position,
                .radius = radius,
                .stretch = std::min(1.0f + drop.speed * k_stretchPerSpeed, k_maxStretch),
                .opacity = drop.opacity * fade});
        }
    }

    void LensRain::wipe(float seconds)
    {
        m_wipeDuration = std::max(seconds, 1e-3f);
        m_wipeSeconds = 0.0f;
    }

    void LensRain::clear()
    {
        m_drops.clear();
        m_view.clear();
        m_landCarry = 0.0f;
        m_wipeDuration = 0.0f;
        m_wipeSeconds = 0.0f;
    }

    void LensRain::submit(Renderer& renderer) const
    {
        renderer.submitLensDroplets(m_view);
    }

    std::span<const LensDroplet> LensRain::droplets() const
    {
        return m_view;
    }

    LensRainStats LensRain::stats() const
    {
        LensRainStats stats{.drops = static_cast<uint32_t>(m_drops.size())};
        float area = 0.0f;

        for (const Drop& drop : m_drops)
        {
            stats.running += drop.speed > 0.0f ? 1u : 0u;
            const float radius = radiusOf(drop.mass);
            area += math::k_pi * radius * radius;
        }

        stats.coverage = std::min(area, 1.0f);
        return stats;
    }
} //namespace lockstep::render
