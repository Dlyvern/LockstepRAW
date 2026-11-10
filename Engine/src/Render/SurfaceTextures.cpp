#include "SurfaceTextures.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>

namespace lockstep::render
{
    namespace
    {
        constexpr std::size_t k_channels{4};
        constexpr float k_pi{3.14159265358979f};
        constexpr int k_detailSize{static_cast<int>(k_surfaceDetailSize)};
        constexpr int k_dirtSize{static_cast<int>(k_lensDirtSize)};

        uint32_t scramble(uint32_t value)
        {
            value ^= value >> 16u;
            value *= 0x7feb352du;
            value ^= value >> 15u;
            value *= 0x846ca68bu;
            value ^= value >> 16u;
            return value;
        }

        float unitFromBits(uint32_t bits)
        {
            return static_cast<float>(bits >> 8u) / 16777216.0f;
        }

        float lattice(int x, int y, uint32_t seed)
        {
            const uint32_t row = scramble(static_cast<uint32_t>(y) + seed * 0x85ebca77u);
            return unitFromBits(scramble(static_cast<uint32_t>(x) * 0x9e3779b1u ^ row));
        }

        int wrap(int value, int period)
        {
            const int remainder = value % period;
            return remainder < 0 ? remainder + period : remainder;
        }

        float smoothStep(float t)
        {
            return t * t * (3.0f - 2.0f * t);
        }

        float valueNoise(float u, float v, int periodX, int periodY, uint32_t seed)
        {
            const float x = u * static_cast<float>(periodX);
            const float y = v * static_cast<float>(periodY);
            const float cellX = std::floor(x);
            const float cellY = std::floor(y);
            const int x0 = wrap(static_cast<int>(cellX), periodX);
            const int y0 = wrap(static_cast<int>(cellY), periodY);
            const int x1 = wrap(x0 + 1, periodX);
            const int y1 = wrap(y0 + 1, periodY);
            const float tx = smoothStep(x - cellX);
            const float ty = smoothStep(y - cellY);
            const float top = lattice(x0, y0, seed) + (lattice(x1, y0, seed) - lattice(x0, y0, seed)) * tx;
            const float bottom = lattice(x0, y1, seed) + (lattice(x1, y1, seed) - lattice(x0, y1, seed)) * tx;

            return top + (bottom - top) * ty;
        }

        float fbm(float u, float v, int frequencyX, int frequencyY, int octaves, uint32_t seed)
        {
            float sum = 0.0f;
            float total = 0.0f;
            float amplitude = 1.0f;

            for (int octave = 0; octave < octaves; ++octave)
            {
                const uint32_t octaveSeed = seed * 131u + static_cast<uint32_t>(octave);
                sum += amplitude * (valueNoise(u, v, frequencyX << octave, frequencyY << octave, octaveSeed) * 2.0f - 1.0f);
                total += amplitude;
                amplitude *= 0.5f;
            }

            return sum / total;
        }

        class Random
        {
        public:
            explicit Random(uint32_t seed)
                : m_state(seed)
            {
            }

            float next()
            {
                m_state = scramble(m_state + 0x9e3779b9u);
                return unitFromBits(m_state);
            }

            float range(float low, float high)
            {
                return low + (high - low) * next();
            }

        private:
            uint32_t m_state;
        };

        struct DetailLayers
        {
            std::vector<float> height;
            std::vector<float> roughness;
            std::vector<float> albedo;
        };

        struct Scratches
        {
            int count{0};
            float minLength{0.0f};
            float maxLength{0.0f};
            float width{1.0f};
            float depth{0.0f};
            float angleSpread{k_pi};
            float roughnessShift{0.0f};
            float albedoShift{0.0f};
        };

        std::size_t texelIndex(int x, int y, int size)
        {
            return static_cast<std::size_t>(wrap(y, size)) * static_cast<std::size_t>(size)
                + static_cast<std::size_t>(wrap(x, size));
        }

        template<typename Sample>
        DetailLayers sampleLayers(Sample sample)
        {
            const std::size_t texels = static_cast<std::size_t>(k_detailSize) * k_detailSize;
            DetailLayers layers{.height = std::vector<float>(texels), .roughness = std::vector<float>(texels),
                .albedo = std::vector<float>(texels)};

            for (int y = 0; y < k_detailSize; ++y)
            {
                for (int x = 0; x < k_detailSize; ++x)
                {
                    const float u = (static_cast<float>(x) + 0.5f) / static_cast<float>(k_detailSize);
                    const float v = (static_cast<float>(y) + 0.5f) / static_cast<float>(k_detailSize);
                    const std::size_t index = texelIndex(x, y, k_detailSize);
                    sample(u, v, layers.height[index], layers.roughness[index], layers.albedo[index]);
                }
            }

            return layers;
        }

        void addPits(DetailLayers& layers, Random& random, int count, float minRadius, float maxRadius, float depth)
        {
            for (int pit = 0; pit < count; ++pit)
            {
                const float centreX = random.range(0.0f, static_cast<float>(k_detailSize));
                const float centreY = random.range(0.0f, static_cast<float>(k_detailSize));
                const float radius = random.range(minRadius, maxRadius);
                const float pitDepth = depth * random.range(0.4f, 1.0f);
                const int reach = static_cast<int>(std::ceil(radius));

                for (int y = static_cast<int>(centreY) - reach; y <= static_cast<int>(centreY) + reach; ++y)
                {
                    for (int x = static_cast<int>(centreX) - reach; x <= static_cast<int>(centreX) + reach; ++x)
                    {
                        const float dx = static_cast<float>(x) + 0.5f - centreX;
                        const float dy = static_cast<float>(y) + 0.5f - centreY;
                        const float falloff = 1.0f - (dx * dx + dy * dy) / (radius * radius);

                        if (falloff <= 0.0f)
                        {
                            continue;
                        }

                        const float weight = falloff * falloff;
                        const std::size_t index = texelIndex(x, y, k_detailSize);
                        layers.height[index] -= pitDepth * weight;
                        layers.roughness[index] += 0.12f * weight;
                        layers.albedo[index] -= 0.15f * weight * pitDepth;
                    }
                }
            }
        }

        void addScratches(DetailLayers& layers, Random& random, const Scratches& scratches)
        {
            std::vector<float> mask(layers.height.size(), 0.0f);
            const int reach = static_cast<int>(std::ceil(scratches.width)) + 1;

            for (int scratch = 0; scratch < scratches.count; ++scratch)
            {
                const float startX = random.range(0.0f, static_cast<float>(k_detailSize));
                const float startY = random.range(0.0f, static_cast<float>(k_detailSize));
                const float angle = random.range(-scratches.angleSpread, scratches.angleSpread);
                const float length = random.range(scratches.minLength, scratches.maxLength);
                const float strength = random.range(0.4f, 1.0f);
                const float directionX = std::cos(angle);
                const float directionY = std::sin(angle);

                for (float travelled = 0.0f; travelled < length; travelled += 0.5f)
                {
                    const float pointX = startX + directionX * travelled;
                    const float pointY = startY + directionY * travelled;
                    const float taper = std::sin(k_pi * travelled / length) * strength;

                    for (int y = static_cast<int>(pointY) - reach; y <= static_cast<int>(pointY) + reach; ++y)
                    {
                        for (int x = static_cast<int>(pointX) - reach; x <= static_cast<int>(pointX) + reach; ++x)
                        {
                            const float dx = static_cast<float>(x) + 0.5f - pointX;
                            const float dy = static_cast<float>(y) + 0.5f - pointY;
                            const float weight = 1.0f - std::sqrt(dx * dx + dy * dy) / scratches.width;

                            if (weight > 0.0f)
                            {
                                float& texel = mask[texelIndex(x, y, k_detailSize)];
                                texel = std::max(texel, weight * taper);
                            }
                        }
                    }
                }
            }

            for (std::size_t index = 0; index < mask.size(); ++index)
            {
                layers.height[index] -= scratches.depth * mask[index];
                layers.roughness[index] += scratches.roughnessShift * mask[index];
                layers.albedo[index] += scratches.albedoShift * mask[index];
            }
        }

        DetailLayers mineralLayers()
        {
            DetailLayers layers = sampleLayers([](float u, float v, float& height, float& roughness, float& albedo)
            {
                height = 0.6f * fbm(u, v, 8, 8, 5, 1) + 0.4f * fbm(u, v, 64, 64, 2, 2);
                roughness = 0.12f * fbm(u, v, 4, 4, 3, 3);
                albedo = 0.08f * fbm(u, v, 6, 6, 4, 4) + 0.04f * fbm(u, v, 96, 96, 1, 5);
            });
            Random random(11u);
            addPits(layers, random, 520, 1.0f, 3.5f, 0.9f);

            return layers;
        }

        DetailLayers paintLayers()
        {
            DetailLayers layers = sampleLayers([](float u, float v, float& height, float& roughness, float& albedo)
            {
                height = 0.6f * fbm(u, v, 24, 24, 2, 6) + 0.25f * fbm(u, v, 96, 96, 1, 7);
                roughness = 0.07f * fbm(u, v, 5, 5, 3, 8);
                albedo = 0.03f * fbm(u, v, 6, 6, 3, 9);
            });
            Random random(23u);
            addScratches(layers, random, Scratches{.count = 36, .minLength = 20.0f, .maxLength = 140.0f, .width = 1.0f,
                .depth = 0.5f, .angleSpread = k_pi, .roughnessShift = 0.12f, .albedoShift = 0.05f});

            return layers;
        }

        DetailLayers metalLayers()
        {
            DetailLayers layers = sampleLayers([](float u, float v, float& height, float& roughness, float& albedo)
            {
                height = 0.45f * fbm(u, v, 2, 64, 2, 10) + 0.15f * fbm(u, v, 16, 16, 3, 11);
                roughness = 0.1f * fbm(u, v, 2, 48, 2, 12) + 0.05f * fbm(u, v, 4, 4, 2, 13);
                albedo = 0.04f * fbm(u, v, 2, 64, 2, 14);
            });
            Random random(37u);
            addScratches(layers, random, Scratches{.count = 110, .minLength = 15.0f, .maxLength = 220.0f, .width = 0.9f,
                .depth = 0.45f, .angleSpread = k_pi, .roughnessShift = -0.08f, .albedoShift = 0.08f});

            return layers;
        }

        DetailLayers fibrousLayers()
        {
            constexpr float k_threads{64.0f};

            return sampleLayers([](float u, float v, float& height, float& roughness, float& albedo)
            {
                const float threadU = u * k_threads;
                const float threadV = v * k_threads;
                const int column = static_cast<int>(threadU);
                const int row = static_cast<int>(threadV);
                const bool weftOnTop = ((column + row) & 1) == 0;
                const float across = weftOnTop ? threadV - std::floor(threadV) : threadU - std::floor(threadU);
                const float thread = std::sin(k_pi * across);
                height = 0.6f * thread + 0.3f * fbm(u, v, 4, 192, 2, 15) + 0.1f * fbm(u, v, 8, 8, 3, 16);
                roughness = 0.05f * fbm(u, v, 8, 8, 2, 17);
                albedo = 0.05f * fbm(u, v, 4, 4, 3, 18) - 0.06f * (1.0f - thread);
            });
        }

        uint8_t toByte(float value)
        {
            return static_cast<uint8_t>(std::clamp(value * 255.0f + 0.5f, 0.0f, 255.0f));
        }

        std::vector<uint8_t> packDetail(const DetailLayers& layers, float bumpScale)
        {
            std::vector<uint8_t> texels(layers.height.size() * k_channels);

            for (int y = 0; y < k_detailSize; ++y)
            {
                for (int x = 0; x < k_detailSize; ++x)
                {
                    const float slopeX = (layers.height[texelIndex(x + 1, y, k_detailSize)]
                        - layers.height[texelIndex(x - 1, y, k_detailSize)]) * 0.5f * bumpScale;
                    const float slopeY = (layers.height[texelIndex(x, y + 1, k_detailSize)]
                        - layers.height[texelIndex(x, y - 1, k_detailSize)]) * 0.5f * bumpScale;
                    const float length = std::sqrt(slopeX * slopeX + slopeY * slopeY + 1.0f);
                    const std::size_t index = texelIndex(x, y, k_detailSize);
                    uint8_t* texel = texels.data() + index * k_channels;
                    texel[0] = toByte(-slopeX / length * 0.5f + 0.5f);
                    texel[1] = toByte(-slopeY / length * 0.5f + 0.5f);
                    texel[2] = toByte(0.5f + layers.roughness[index]);
                    texel[3] = toByte(0.5f + layers.albedo[index]);
                }
            }

            return texels;
        }

        struct DirtField
        {
            std::vector<float> rgb;

            void add(int x, int y, float red, float green, float blue)
            {
                if (x < 0 || y < 0 || x >= k_dirtSize || y >= k_dirtSize)
                {
                    return;
                }

                float* texel = rgb.data() + (static_cast<std::size_t>(y) * k_dirtSize + static_cast<std::size_t>(x)) * 3;
                texel[0] += red;
                texel[1] += green;
                texel[2] += blue;
            }
        };

        template<typename Shape>
        void splat(DirtField& field, float centreX, float centreY, float reach, float tintBlue, Shape shape)
        {
            const int left = static_cast<int>(centreX - reach);
            const int top = static_cast<int>(centreY - reach);
            const int right = static_cast<int>(centreX + reach);
            const int bottom = static_cast<int>(centreY + reach);

            for (int y = top; y <= bottom; ++y)
            {
                for (int x = left; x <= right; ++x)
                {
                    const float value = shape(static_cast<float>(x) + 0.5f - centreX, static_cast<float>(y) + 0.5f - centreY);

                    if (value > 0.0f)
                    {
                        field.add(x, y, value, value * 0.97f, value * tintBlue);
                    }
                }
            }
        }

        float mottle(float x, float y)
        {
            const float size = static_cast<float>(k_dirtSize);
            return 0.55f + 0.45f * fbm(x / size, y / size, 12, 12, 3, 41);
        }
    } //namespace

    std::vector<uint8_t> surfaceDetailTexels(SurfaceDetail kind)
    {
        switch (kind)
        {
            case SurfaceDetail::Mineral:
            {
                return packDetail(mineralLayers(), 2.5f);
            }
            case SurfaceDetail::Paint:
            {
                return packDetail(paintLayers(), 1.5f);
            }
            case SurfaceDetail::Metal:
            {
                return packDetail(metalLayers(), 1.5f);
            }
            case SurfaceDetail::Fibrous:
            case SurfaceDetail::None:
            {
                return packDetail(fibrousLayers(), 1.0f);
            }
        }

        return packDetail(fibrousLayers(), 1.0f);
    }

    std::vector<uint8_t> lensDirtTexels()
    {
        const std::size_t texels = static_cast<std::size_t>(k_dirtSize) * k_dirtSize;
        DirtField field{.rgb = std::vector<float>(texels * 3, 0.0f)};
        const float size = static_cast<float>(k_dirtSize);

        for (int y = 0; y < k_dirtSize; ++y)
        {
            for (int x = 0; x < k_dirtSize; ++x)
            {
                const float haze = 0.02f + 0.03f * std::max(fbm(static_cast<float>(x) / size,
                    static_cast<float>(y) / size, 3, 3, 4, 40), 0.0f);
                field.add(x, y, haze, haze, haze);
            }
        }

        Random random(97u);

        for (int blob = 0; blob < 90; ++blob)
        {
            const float centreX = random.range(0.0f, size);
            const float centreY = random.range(0.0f, size);
            const float radius = random.range(8.0f, 70.0f);
            const float intensity = random.range(0.05f, 0.35f) * (1.2f - radius / 80.0f);
            const float sigma = radius / 2.5f;
            splat(field, centreX, centreY, radius, random.range(0.85f, 1.0f), [&](float dx, float dy)
            {
                return intensity * std::exp(-(dx * dx + dy * dy) / (2.0f * sigma * sigma))
                    * mottle(centreX + dx, centreY + dy);
            });
        }

        for (int ring = 0; ring < 12; ++ring)
        {
            const float radius = random.range(6.0f, 22.0f);
            const float intensity = random.range(0.05f, 0.12f);
            splat(field, random.range(0.0f, size), random.range(0.0f, size), radius + 3.0f, 0.95f,
                [&](float dx, float dy)
            {
                const float offset = std::sqrt(dx * dx + dy * dy) - radius;
                return intensity * std::exp(-offset * offset / 2.0f);
            });
        }

        for (int speck = 0; speck < 420; ++speck)
        {
            const float radius = random.range(0.6f, 2.2f);
            const float intensity = random.range(0.3f, 1.0f);
            splat(field, random.range(0.0f, size), random.range(0.0f, size), radius + 1.0f, 1.0f,
                [&](float dx, float dy)
            {
                return intensity * std::max(1.0f - std::sqrt(dx * dx + dy * dy) / radius, 0.0f);
            });
        }

        for (int smear = 0; smear < 6; ++smear)
        {
            const float angle = random.range(0.0f, k_pi);
            const float length = random.range(80.0f, 220.0f);
            const float width = random.range(6.0f, 14.0f);
            const float intensity = random.range(0.08f, 0.18f);
            const float alongX = std::cos(angle);
            const float alongY = std::sin(angle);
            splat(field, random.range(0.0f, size), random.range(0.0f, size), length * 0.5f + width * 2.0f, 0.9f,
                [&](float dx, float dy)
            {
                const float along = (dx * alongX + dy * alongY) / (length * 0.5f);
                const float across = (-dx * alongY + dy * alongX) / width;
                return intensity * std::max(1.0f - along * along, 0.0f) * std::exp(-across * across);
            });
        }

        std::vector<uint8_t> result(texels * k_channels);

        for (std::size_t index = 0; index < texels; ++index)
        {
            result[index * k_channels + 0] = toByte(field.rgb[index * 3 + 0]);
            result[index * k_channels + 1] = toByte(field.rgb[index * 3 + 1]);
            result[index * k_channels + 2] = toByte(field.rgb[index * 3 + 2]);
            result[index * k_channels + 3] = 255;
        }

        return result;
    }
} //namespace lockstep::render
