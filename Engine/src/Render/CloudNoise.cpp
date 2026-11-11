#include "CloudNoise.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace lockstep::render
{
    namespace
    {
        constexpr uint32_t k_channels{4};

        uint32_t hash(uint32_t x, uint32_t y, uint32_t seed)
        {
            uint32_t value = x * 0x8DA6B343u ^ y * 0xD8163841u ^ seed * 0xCB1AB31Fu;
            value ^= value >> 13;
            value *= 0x5BD1E995u;
            value ^= value >> 15;
            return value;
        }

        float unitHash(uint32_t x, uint32_t y, uint32_t seed)
        {
            return static_cast<float>(hash(x, y, seed) >> 8) * (1.0f / 16777216.0f);
        }

        uint32_t wrapped(int32_t cell, uint32_t period)
        {
            const auto signedPeriod = static_cast<int32_t>(period);
            return static_cast<uint32_t>(((cell % signedPeriod) + signedPeriod) % signedPeriod);
        }

        float smooth(float t)
        {
            return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
        }

        float gradientNoise(float u, float v, uint32_t period, uint32_t seed)
        {
            const float x = u * static_cast<float>(period);
            const float y = v * static_cast<float>(period);
            const auto cellX = static_cast<int32_t>(std::floor(x));
            const auto cellY = static_cast<int32_t>(std::floor(y));
            const float fx = x - static_cast<float>(cellX);
            const float fy = y - static_cast<float>(cellY);
            std::array<float, 4> corners{};

            for (uint32_t corner = 0; corner < 4; ++corner)
            {
                const uint32_t ox = corner & 1u;
                const uint32_t oy = corner >> 1u;
                const float angle = unitHash(wrapped(cellX + static_cast<int32_t>(ox), period),
                    wrapped(cellY + static_cast<int32_t>(oy), period), seed) * 6.2831853f;
                corners[corner] = std::cos(angle) * (fx - static_cast<float>(ox))
                    + std::sin(angle) * (fy - static_cast<float>(oy));
            }

            const float sx = smooth(fx);
            const float sy = smooth(fy);
            const float bottom = corners[0] + (corners[1] - corners[0]) * sx;
            const float top = corners[2] + (corners[3] - corners[2]) * sx;
            return (bottom + (top - bottom) * sy) * 0.7071f + 0.5f;
        }

        float cellNoise(float u, float v, uint32_t period, uint32_t seed)
        {
            const float x = u * static_cast<float>(period);
            const float y = v * static_cast<float>(period);
            const auto cellX = static_cast<int32_t>(std::floor(x));
            const auto cellY = static_cast<int32_t>(std::floor(y));
            float nearest = std::numeric_limits<float>::max();

            for (int32_t oy = -1; oy <= 1; ++oy)
            {
                for (int32_t ox = -1; ox <= 1; ++ox)
                {
                    const uint32_t wx = wrapped(cellX + ox, period);
                    const uint32_t wy = wrapped(cellY + oy, period);
                    const float px = static_cast<float>(cellX + ox) + unitHash(wx, wy, seed);
                    const float py = static_cast<float>(cellY + oy) + unitHash(wx, wy, seed + 17u);
                    nearest = std::min(nearest, (px - x) * (px - x) + (py - y) * (py - y));
                }
            }

            return 1.0f - std::min(std::sqrt(nearest), 1.0f);
        }

        float fractal(float u, float v, uint32_t period, uint32_t octaves, uint32_t seed)
        {
            float sum = 0.0f;
            float amplitude = 0.5f;
            float total = 0.0f;

            for (uint32_t octave = 0; octave < octaves; ++octave)
            {
                sum += gradientNoise(u, v, period << octave, seed + octave) * amplitude;
                total += amplitude;
                amplitude *= 0.5f;
            }

            return sum / total;
        }

        void normalizeChannel(std::vector<float>& values, uint32_t channel)
        {
            float low = std::numeric_limits<float>::max();
            float high = std::numeric_limits<float>::lowest();

            for (std::size_t index = channel; index < values.size(); index += k_channels)
            {
                low = std::min(low, values[index]);
                high = std::max(high, values[index]);
            }

            const float range = std::max(high - low, 1e-6f);

            for (std::size_t index = channel; index < values.size(); index += k_channels)
            {
                values[index] = (values[index] - low) / range;
            }
        }
    } //namespace

    std::vector<uint8_t> cloudNoiseTexels()
    {
        std::vector<float> values(static_cast<std::size_t>(k_cloudNoiseSize) * k_cloudNoiseSize * k_channels);

        for (uint32_t y = 0; y < k_cloudNoiseSize; ++y)
        {
            for (uint32_t x = 0; x < k_cloudNoiseSize; ++x)
            {
                const float u = (static_cast<float>(x) + 0.5f) / static_cast<float>(k_cloudNoiseSize);
                const float v = (static_cast<float>(y) + 0.5f) / static_cast<float>(k_cloudNoiseSize);
                const std::size_t base = (static_cast<std::size_t>(y) * k_cloudNoiseSize + x) * k_channels;
                const float billow = cellNoise(u, v, 5, 3u) * 0.6f + cellNoise(u, v, 11, 7u) * 0.4f;
                values[base + 0] = fractal(u, v, 4, 5, 11u) * 0.6f + billow * 0.4f;
                values[base + 1] = fractal(u, v, 12, 3, 29u);
                values[base + 2] = fractal(u, v, 16, 3, 43u) * 0.5f + cellNoise(u, v, 24, 51u) * 0.5f;
                values[base + 3] = cellNoise(u, v, 8, 63u);
            }
        }

        for (uint32_t channel = 0; channel < k_channels; ++channel)
        {
            normalizeChannel(values, channel);
        }

        std::vector<uint8_t> texels(values.size());
        std::transform(values.begin(), values.end(), texels.begin(),
            [](float value) { return static_cast<uint8_t>(std::lround(std::clamp(value, 0.0f, 1.0f) * 255.0f)); });

        return texels;
    }
} //namespace lockstep::render
