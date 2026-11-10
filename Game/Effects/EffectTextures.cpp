#include "Effects/EffectTextures.hpp"

#include <algorithm>
#include <cmath>

namespace game
{
    namespace
    {
        constexpr float k_twoPi{6.28318531f};
        constexpr uint32_t k_smokeSeed{17};
        constexpr uint32_t k_chunkSeed{91};
        constexpr uint32_t k_chunkCorners{9};

        struct Texel
        {
            float red{1.0f};
            float green{1.0f};
            float blue{1.0f};
            float alpha{0.0f};
        };

        float lattice(int32_t x, int32_t y, uint32_t seed)
        {
            uint32_t hash = static_cast<uint32_t>(x) * 374761393u + static_cast<uint32_t>(y) * 668265263u
                + seed * 2246822519u;
            hash = (hash ^ (hash >> 13)) * 1274126177u;
            hash ^= hash >> 16;
            return static_cast<float>(hash & 0xFFFFFFu) * (1.0f / 16777216.0f);
        }

        float smooth(float t)
        {
            return t * t * (3.0f - 2.0f * t);
        }

        float valueNoise(float x, float y, uint32_t seed)
        {
            const float cellX = std::floor(x);
            const float cellY = std::floor(y);
            const auto ix = static_cast<int32_t>(cellX);
            const auto iy = static_cast<int32_t>(cellY);
            const float fx = smooth(x - cellX);
            const float fy = smooth(y - cellY);
            const float top = lattice(ix, iy, seed) + (lattice(ix + 1, iy, seed) - lattice(ix, iy, seed)) * fx;
            const float bottom = lattice(ix, iy + 1, seed) + (lattice(ix + 1, iy + 1, seed) - lattice(ix, iy + 1, seed)) * fx;

            return top + (bottom - top) * fy;
        }

        float fractalNoise(float x, float y, uint32_t seed)
        {
            float sum = 0.0f;
            float amplitude = 0.5f;
            float frequency = 1.0f;

            for (uint32_t octave = 0; octave < 4; ++octave)
            {
                sum += valueNoise(x * frequency, y * frequency, seed + octave * 31u) * amplitude;
                amplitude *= 0.5f;
                frequency *= 2.0f;
            }

            return sum / 0.9375f;
        }

        Texel smokeTexel(float u, float v, uint32_t variant)
        {
            const float dx = u - 0.5f;
            const float dy = v - 0.5f;
            const float radius = std::sqrt(dx * dx + dy * dy) * 2.0f;
            const uint32_t seed = k_smokeSeed + variant * 13u;
            const float noise = fractalNoise(u * 5.0f, v * 5.0f, seed);
            const float edge = std::clamp(1.0f - radius * (0.75f + 0.5f * noise), 0.0f, 1.0f);
            const float density = smooth(edge) * std::clamp(0.35f + 0.9f * noise, 0.0f, 1.0f);
            const float shade = 0.82f + 0.18f * std::clamp(1.0f - v + noise * 0.4f - 0.2f, 0.0f, 1.0f);

            return Texel{.red = shade, .green = shade, .blue = shade, .alpha = std::clamp(density * 1.4f, 0.0f, 1.0f)};
        }

        Texel dotTexel(float u, float v, float sharpness)
        {
            const float dx = (u - 0.5f) * 2.0f;
            const float dy = (v - 0.5f) * 2.0f;
            const float radius2 = dx * dx + dy * dy;
            const float falloff = std::exp(-radius2 * sharpness) * std::clamp(1.0f - radius2, 0.0f, 1.0f);

            return Texel{.alpha = falloff};
        }

        Texel chunkTexel(float u, float v, uint32_t variant)
        {
            const float dx = (u - 0.5f) * 2.0f;
            const float dy = (v - 0.5f) * 2.0f;
            const float angle = std::atan2(dy, dx);
            const float radius = std::sqrt(dx * dx + dy * dy);
            const uint32_t seed = k_chunkSeed + variant * 7u;
            const float sector = (angle / k_twoPi + 0.5f) * static_cast<float>(k_chunkCorners);
            const auto corner = static_cast<int32_t>(std::floor(sector));
            const float blend = sector - std::floor(sector);
            const float from = 0.55f + 0.35f * lattice(corner % static_cast<int32_t>(k_chunkCorners), 0, seed);
            const float to = 0.55f + 0.35f * lattice((corner + 1) % static_cast<int32_t>(k_chunkCorners), 0, seed);
            const float outline = from + (to - from) * blend;
            const float coverage = std::clamp((outline - radius) * 40.0f, 0.0f, 1.0f);
            const float facet = 0.55f + 0.45f * lattice(corner, 1, seed);
            const float light = std::clamp(facet - dy * 0.25f + (fractalNoise(u * 12.0f, v * 12.0f, seed) - 0.5f) * 0.3f,
                0.25f, 1.0f);

            return Texel{.red = light, .green = light, .blue = light, .alpha = coverage};
        }

        Texel cellTexel(uint32_t frame, float u, float v)
        {
            if (frame >= k_smokeFrame && frame < k_smokeFrame + k_smokeFrames)
            {
                return smokeTexel(u, v, frame - k_smokeFrame);
            }

            if (frame == k_dotFrame)
            {
                return dotTexel(u, v, 3.0f);
            }

            if (frame == k_sparkFrame)
            {
                return dotTexel(u, v, 9.0f);
            }

            if (frame >= k_chunkFrame && frame < k_chunkFrame + k_chunkFrames)
            {
                return chunkTexel(u, v, frame - k_chunkFrame);
            }

            return Texel{};
        }

        std::byte encodeSrgb(float linear)
        {
            const float clamped = std::clamp(linear, 0.0f, 1.0f);
            const float srgb = clamped <= 0.0031308f ? clamped * 12.92f : 1.055f * std::pow(clamped, 1.0f / 2.4f) - 0.055f;

            return static_cast<std::byte>(std::lround(srgb * 255.0f));
        }
    } //namespace

    std::vector<std::byte> effectAtlasPixels()
    {
        std::vector<std::byte> pixels(static_cast<std::size_t>(k_effectAtlasSize) * k_effectAtlasSize * 4);

        for (uint32_t y = 0; y < k_effectAtlasSize; ++y)
        {
            for (uint32_t x = 0; x < k_effectAtlasSize; ++x)
            {
                const uint32_t frame = (y / k_effectCellSize) * k_effectAtlasColumns + x / k_effectCellSize;
                const float u = (static_cast<float>(x % k_effectCellSize) + 0.5f) / static_cast<float>(k_effectCellSize);
                const float v = (static_cast<float>(y % k_effectCellSize) + 0.5f) / static_cast<float>(k_effectCellSize);
                const Texel texel = cellTexel(frame, u, v);
                const std::size_t offset = (static_cast<std::size_t>(y) * k_effectAtlasSize + x) * 4;

                pixels[offset] = encodeSrgb(texel.red);
                pixels[offset + 1] = encodeSrgb(texel.green);
                pixels[offset + 2] = encodeSrgb(texel.blue);
                pixels[offset + 3] = static_cast<std::byte>(std::lround(std::clamp(texel.alpha, 0.0f, 1.0f) * 255.0f));
            }
        }

        return pixels;
    }
} //namespace game
