#include "Gore/BloodTextures.hpp"

#include <algorithm>
#include <array>
#include <cmath>

namespace game
{
    namespace
    {
        constexpr float k_twoPi{6.28318531f};
        constexpr float k_reach{0.48f};
        constexpr std::array<float, 3> k_thinColour{132.0f, 9.0f, 7.0f};
        constexpr std::array<float, 3> k_thickColour{52.0f, 2.0f, 2.0f};
        constexpr float k_maxAlpha{0.97f};

        class Random
        {
        public:
            explicit Random(uint32_t seed)
                : m_state(seed * 747796405u + 2891336453u)
            {
                next();
            }

            float next()
            {
                m_state ^= m_state << 13;
                m_state ^= m_state >> 17;
                m_state ^= m_state << 5;
                return static_cast<float>(m_state >> 8) * (1.0f / 16777216.0f);
            }

            float range(float low, float high)
            {
                return low + (high - low) * next();
            }

        private:
            uint32_t m_state{1};
        };

        float lattice(int32_t x, int32_t y, uint32_t seed)
        {
            uint32_t hash = static_cast<uint32_t>(x) * 374761393u + static_cast<uint32_t>(y) * 668265263u + seed * 2246822519u;
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
                sum += amplitude * valueNoise(x * frequency, y * frequency, seed + octave);
                amplitude *= 0.5f;
                frequency *= 2.0f;
            }

            return sum / 0.9375f;
        }

        float smoothStep(float edge0, float edge1, float value)
        {
            const float t = std::clamp((value - edge0) / (edge1 - edge0), 0.0f, 1.0f);
            return smooth(t);
        }

        struct Field
        {
            uint32_t size{0};
            std::vector<float> thickness;
        };

        template<typename Shape>
        void stamp(Field& field, float minX, float minY, float maxX, float maxY, Shape shape)
        {
            const auto size = static_cast<float>(field.size);
            const auto clampPixel = [&](float value) { return static_cast<uint32_t>(std::clamp(value * size, 0.0f, size - 1.0f)); };
            const uint32_t x0 = clampPixel(minX);
            const uint32_t x1 = clampPixel(maxX);
            const uint32_t y0 = clampPixel(minY);
            const uint32_t y1 = clampPixel(maxY);

            for (uint32_t y = y0; y <= y1; ++y)
            {
                for (uint32_t x = x0; x <= x1; ++x)
                {
                    const float u = (static_cast<float>(x) + 0.5f) / size;
                    const float v = (static_cast<float>(y) + 0.5f) / size;
                    float& value = field.thickness[static_cast<std::size_t>(y) * field.size + x];
                    value = std::max(value, shape(u, v));
                }
            }
        }

        void blob(Field& field, float centreX, float centreY, float radius, float roughness, float frequency,
            float thickness, uint32_t seed)
        {
            const float reach = radius * (1.0f + roughness);
            stamp(field, centreX - reach, centreY - reach, centreX + reach, centreY + reach, [=](float u, float v)
            {
                const float dx = u - centreX;
                const float dy = v - centreY;
                const float edge = radius * (1.0f - 0.5f * roughness + roughness * fractalNoise(u * frequency, v * frequency, seed));
                const float distance = std::sqrt(dx * dx + dy * dy) / std::max(edge, 1.0e-4f);
                return smoothStep(1.0f, 0.82f, distance) * thickness * (1.0f - 0.35f * distance * distance);
            });
        }

        void streak(Field& field, float fromX, float fromY, float toX, float toY, float startWidth, float endWidth,
            float thickness)
        {
            const float reach = std::max(startWidth, endWidth);
            stamp(field, std::min(fromX, toX) - reach, std::min(fromY, toY) - reach, std::max(fromX, toX) + reach,
                std::max(fromY, toY) + reach, [=](float u, float v)
            {
                const float segmentX = toX - fromX;
                const float segmentY = toY - fromY;
                const float lengthSquared = std::max(segmentX * segmentX + segmentY * segmentY, 1.0e-8f);
                const float t = std::clamp(((u - fromX) * segmentX + (v - fromY) * segmentY) / lengthSquared, 0.0f, 1.0f);
                const float dx = u - (fromX + segmentX * t);
                const float dy = v - (fromY + segmentY * t);
                const float width = startWidth + (endWidth - startWidth) * t;
                const float distance = std::sqrt(dx * dx + dy * dy) / width;
                return smoothStep(1.0f, 0.7f, distance) * thickness * (1.0f - 0.4f * t);
            });
        }

        void splatter(Field& field, Random& random, uint32_t seed)
        {
            const float radius = random.range(0.13f, 0.18f);
            blob(field, 0.5f, 0.5f, radius, 0.7f, 9.0f, 1.0f, seed);

            for (uint32_t index = 0; index < 11; ++index)
            {
                const float angle = random.range(0.0f, k_twoPi);
                const float length = random.range(0.24f, k_reach - 0.03f);
                const float endX = 0.5f + std::cos(angle) * length;
                const float endY = 0.5f + std::sin(angle) * length;
                const float width = random.range(0.012f, 0.03f);
                streak(field, 0.5f + std::cos(angle) * radius * 0.6f, 0.5f + std::sin(angle) * radius * 0.6f, endX, endY,
                    width, width * 0.2f, 0.75f);
                blob(field, endX, endY, width * random.range(0.7f, 1.4f), 0.4f, 40.0f, 0.8f, seed + index);
            }

            for (uint32_t index = 0; index < 40; ++index)
            {
                const float angle = random.range(0.0f, k_twoPi);
                const float distance = random.range(radius * 0.9f, k_reach - 0.02f);
                const float size = random.range(0.003f, 0.022f) * (1.2f - distance);
                blob(field, 0.5f + std::cos(angle) * distance, 0.5f + std::sin(angle) * distance, size, 0.3f, 60.0f,
                    random.range(0.5f, 0.9f), seed + 100 + index);
            }
        }

        void pool(Field& field, Random& random, uint32_t seed)
        {
            blob(field, 0.5f, 0.5f, 0.3f, 0.45f, 3.5f, 1.15f, seed);

            for (uint32_t index = 0; index < 6; ++index)
            {
                const float angle = random.range(0.0f, k_twoPi);
                const float distance = random.range(0.1f, 0.2f);
                blob(field, 0.5f + std::cos(angle) * distance, 0.5f + std::sin(angle) * distance,
                    random.range(0.1f, 0.18f), 0.5f, 5.0f, 1.1f, seed + index + 1);
            }
        }

        void drop(Field& field, Random& random, uint32_t seed)
        {
            blob(field, 0.5f, 0.5f, 0.2f, 0.35f, 6.0f, 0.95f, seed);

            for (uint32_t index = 0; index < 8; ++index)
            {
                const float angle = random.range(0.0f, k_twoPi);
                const float distance = random.range(0.25f, 0.42f);
                blob(field, 0.5f + std::cos(angle) * distance, 0.5f + std::sin(angle) * distance,
                    random.range(0.015f, 0.04f), 0.3f, 30.0f, 0.8f, seed + index + 1);
            }
        }

        std::byte channel(float value)
        {
            return static_cast<std::byte>(static_cast<uint8_t>(std::clamp(value, 0.0f, 255.0f) + 0.5f));
        }
    } //namespace

    std::vector<std::byte> bloodTexture(BloodTextureKind kind, uint32_t size, uint32_t seed)
    {
        Field field{.size = size, .thickness = std::vector<float>(static_cast<std::size_t>(size) * size, 0.0f)};
        Random random(seed);

        switch (kind)
        {
            case BloodTextureKind::Splatter:
                splatter(field, random, seed);
                break;
            case BloodTextureKind::Pool:
                pool(field, random, seed);
                break;
            case BloodTextureKind::Drop:
                drop(field, random, seed);
                break;
        }

        std::vector<std::byte> pixels;
        pixels.reserve(field.thickness.size() * 4);

        for (std::size_t index = 0; index < field.thickness.size(); ++index)
        {
            const float thickness = field.thickness[index];
            const float u = static_cast<float>(index % size) / static_cast<float>(size);
            const float v = static_cast<float>(index / size) / static_cast<float>(size);
            const float grain = 0.88f + 0.24f * fractalNoise(u * 48.0f, v * 48.0f, seed + 7);
            const float depth = smoothStep(0.25f, 1.0f, thickness);

            for (std::size_t component = 0; component < 3; ++component)
            {
                pixels.push_back(channel((k_thinColour[component] + (k_thickColour[component] - k_thinColour[component]) * depth)
                    * grain));
            }

            pixels.push_back(channel(255.0f * k_maxAlpha * smoothStep(0.03f, 0.3f, thickness)));
        }

        return pixels;
    }
} //namespace game
