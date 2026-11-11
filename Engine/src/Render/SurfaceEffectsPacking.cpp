#include "SurfaceEffectsPacking.hpp"

#include "Lockstep/Render/Renderer.hpp"

#include "RendererState.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

namespace lockstep::render
{
    namespace
    {
        constexpr float k_timeWrap{3600.0f};
        constexpr float k_maxRippleStrength{2.0f};
        constexpr float k_maxPuddleRadius{3.0f};
        constexpr float k_minDropInterval{0.05f};

        float finiteOr(float value, float fallback)
        {
            return std::isfinite(value) ? value : fallback;
        }

        void store(float (&target)[4], float x, float y, float z, float w)
        {
            target[0] = x;
            target[1] = y;
            target[2] = z;
            target[3] = w;
        }
    } //namespace

    void Renderer::submitDripPuddle(const DripPuddle& puddle)
    {
        m_state->dripPuddles.push_back(puddle);
    }

    void packSurfaceEffects(RendererState& state)
    {
        const SurfaceEffectSettings& effects = state.settings.surfaceEffects;
        GpuFrameLighting& gpu = state.prepared.lighting;
        std::vector<DripPuddle>& puddles = state.dripPuddles;
        uint32_t count = 0;

        if (effects.dripPuddles && !puddles.empty())
        {
            const math::Vec3 camera = state.camera.position;
            const std::size_t kept = std::min<std::size_t>(puddles.size(), k_maxDripPuddles);
            std::partial_sort(puddles.begin(), puddles.begin() + static_cast<std::ptrdiff_t>(kept), puddles.end(),
                [&camera](const DripPuddle& a, const DripPuddle& b)
                {
                    return math::length(a.position - camera) < math::length(b.position - camera);
                });

            for (std::size_t index = 0; index < kept; ++index)
            {
                const DripPuddle& puddle = puddles[index];
                store(gpu.dripPuddles[count * 2], puddle.position.x, puddle.position.y, puddle.position.z,
                    std::clamp(finiteOr(puddle.radius, 0.0f), 0.0f, k_maxPuddleRadius));
                store(gpu.dripPuddles[count * 2 + 1], std::max(finiteOr(puddle.dropAge, 0.0f), 0.0f),
                    std::max(finiteOr(puddle.interval, 1.0f), k_minDropInterval),
                    std::clamp(finiteOr(puddle.strength, 0.0f), 0.0f, 1.0f), 0.0f);
                ++count;
            }
        }

        store(gpu.surfaceEffects, std::fmod(std::max(finiteOr(effects.time, 0.0f), 0.0f), k_timeWrap),
            static_cast<float>(count), effects.screens ? std::clamp(effects.scanlines, 0.0f, 1.0f) : 0.0f,
            std::clamp(effects.rippleStrength, 0.0f, k_maxRippleStrength));
    }
} //namespace lockstep::render
