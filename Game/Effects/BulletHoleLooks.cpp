#include "Effects/BulletHoleLooks.hpp"

#include <algorithm>
#include <array>
#include <cmath>

namespace game
{
    namespace
    {
        constexpr float k_pi{3.14159265f};
        constexpr float k_twoPi{6.2831853f};

        struct Texel
        {
            float red{0.0f};
            float green{0.0f};
            float blue{0.0f};
            float alpha{0.0f};
        };

        struct Point
        {
            float u{0.0f};
            float v{0.0f};
            float radius{0.0f};
            float angle{0.0f};
            uint32_t x{0};
            uint32_t y{0};
        };

        float hash(uint32_t x, uint32_t y)
        {
            uint32_t value = x * 374761393u + y * 668265263u;
            value = (value ^ (value >> 13u)) * 1274126177u;
            return static_cast<float>(value ^ (value >> 16u)) / 4294967295.0f;
        }

        float smooth(float edge0, float edge1, float value)
        {
            const float t = std::clamp((value - edge0) / (edge1 - edge0), 0.0f, 1.0f);
            return t * t * (3.0f - 2.0f * t);
        }

        float angleNoise(float angle, float cells, uint32_t seed)
        {
            const float position = (angle + k_pi) / k_twoPi * cells;
            const float cell = std::floor(position);
            const float blend = smooth(0.0f, 1.0f, position - cell);
            const auto count = static_cast<uint32_t>(cells);
            const auto first = static_cast<uint32_t>(cell) % count;

            return hash(first, seed) * (1.0f - blend) + hash((first + 1u) % count, seed) * blend;
        }

        float valueNoise(float u, float v, float scale, uint32_t seed)
        {
            const float x = (u + 1.0f) * scale;
            const float y = (v + 1.0f) * scale;
            const float cellX = std::floor(x);
            const float cellY = std::floor(y);
            const float blendX = smooth(0.0f, 1.0f, x - cellX);
            const float blendY = smooth(0.0f, 1.0f, y - cellY);
            const auto ix = static_cast<uint32_t>(cellX) + seed * 31u;
            const auto iy = static_cast<uint32_t>(cellY);
            const float top = hash(ix, iy) * (1.0f - blendX) + hash(ix + 1u, iy) * blendX;
            const float bottom = hash(ix, iy + 1u) * (1.0f - blendX) + hash(ix + 1u, iy + 1u) * blendX;

            return top * (1.0f - blendY) + bottom * blendY;
        }

        void over(Texel& texel, float red, float green, float blue, float alpha)
        {
            const float coverage = std::clamp(alpha, 0.0f, 1.0f);
            texel.red = red * coverage + texel.red * (1.0f - coverage);
            texel.green = green * coverage + texel.green * (1.0f - coverage);
            texel.blue = blue * coverage + texel.blue * (1.0f - coverage);
            texel.alpha = coverage + texel.alpha * (1.0f - coverage);
        }

        float angleBetween(float a, float b)
        {
            return std::abs(std::remainder(a - b, k_twoPi));
        }

        float cracks(const Point& point, uint32_t count, float reach, float width, uint32_t seed)
        {
            float strongest = 0.0f;

            for (uint32_t index = 0; index < count; ++index)
            {
                const float base = (static_cast<float>(index) + hash(index, seed) * 0.6f) / static_cast<float>(count)
                    * k_twoPi - k_pi;
                const float length = reach * (0.55f + 0.45f * hash(index, seed + 1u));
                const float wiggle = 0.12f * std::sin(point.radius * (9.0f + 5.0f * hash(index, seed + 2u)) + base * 3.0f)
                    + 0.05f * std::sin(point.radius * 31.0f + static_cast<float>(index));
                const float across = point.radius * angleBetween(point.angle, base + wiggle);
                const float line = 1.0f - smooth(width * 0.4f, width, across);
                const float tip = 1.0f - smooth(length * 0.7f, length, point.radius);
                strongest = std::max(strongest, line * tip);
            }

            return strongest;
        }

        Texel concrete(const Point& point)
        {
            Texel texel{};
            const float grain = hash(point.x, point.y);
            const float jag = 0.8f + 0.4f * angleNoise(point.angle, 14.0f, 7u);
            const float radius = point.radius / jag;
            const float scorch = (1.0f - smooth(0.35f, 0.9f, radius)) * (0.5f + 0.5f * grain);
            const float crater = 1.0f - smooth(0.16f, 0.42f, radius);
            const float dust = 0.13f + 0.1f * grain;
            over(texel, 0.04f, 0.038f, 0.035f, scorch * 0.3f);
            over(texel, dust, dust * 0.97f, dust * 0.93f, crater * 0.75f);
            over(texel, 0.015f, 0.014f, 0.013f, 1.0f - smooth(0.1f, 0.16f, radius));

            return texel;
        }

        Texel plaster(const Point& point)
        {
            Texel texel{};
            const float grain = hash(point.x, point.y);
            const float jag = 0.7f + 0.6f * angleNoise(point.angle, 11.0f, 3u);
            const float radius = point.radius / jag;
            const float powder = (1.0f - smooth(0.4f, 0.95f, radius)) * (0.4f + 0.6f * valueNoise(point.u, point.v, 9.0f, 5u));
            const float spall = 1.0f - smooth(0.36f, 0.42f, radius);
            const float gypsum = 0.5f + 0.1f * grain;
            const float shade = 0.75f + 0.25f * smooth(0.12f, 0.4f, radius);
            over(texel, 0.6f, 0.59f, 0.56f, powder * 0.35f);
            over(texel, gypsum * shade, gypsum * shade * 0.98f, gypsum * shade * 0.94f, spall);
            over(texel, 0.05f, 0.048f, 0.045f, 1.0f - smooth(0.1f, 0.15f, point.radius / (0.9f + 0.2f * jag)));

            return texel;
        }

        Texel tile(const Point& point)
        {
            Texel texel{};
            const float grain = hash(point.x, point.y);
            const float jag = 0.8f + 0.4f * angleNoise(point.angle, 9.0f, 11u);
            const float radius = point.radius / jag;
            const float crack = cracks(point, 6, 0.95f, 0.035f, 13u);
            const float chipped = 1.0f - smooth(0.22f, 0.3f, radius);
            const float body = 0.4f + 0.12f * grain;
            over(texel, 0.12f, 0.12f, 0.12f, crack * 0.85f);
            over(texel, body, body * 0.97f, body * 0.92f, chipped);
            over(texel, 0.03f, 0.03f, 0.03f, 1.0f - smooth(0.08f, 0.13f, radius));

            return texel;
        }

        Texel wood(const Point& point)
        {
            Texel texel{};
            const float fibre = valueNoise(point.u * 0.15f, point.v, 22.0f, 17u);
            const float length = 0.35f + 0.5f * valueNoise(0.0f, point.v, 14.0f, 19u);
            const float width = 0.26f * (1.0f - smooth(0.0f, length, std::abs(point.u)));
            const float splinter = (1.0f - smooth(width * 0.6f, width, std::abs(point.v))) * smooth(0.3f, 0.6f, fibre + 0.25f);
            const float torn = 1.0f - smooth(0.14f, 0.24f, point.radius / (0.85f + 0.3f * angleNoise(point.angle, 8.0f, 21u)));
            const float raw = 0.38f + 0.15f * fibre;
            over(texel, raw, raw * 0.78f, raw * 0.52f, std::max(splinter, torn * 0.9f));
            over(texel, 0.025f, 0.018f, 0.012f, 1.0f - smooth(0.09f, 0.14f, point.radius));

            return texel;
        }

        Texel metal(const Point& point)
        {
            Texel texel{};
            const float grain = hash(point.x, point.y);
            const float jag = 0.85f + 0.3f * angleNoise(point.angle, 12.0f, 23u);
            const float radius = point.radius / jag;
            const float paint = 1.0f - smooth(0.36f, 0.44f, radius);
            const float bare = 1.0f - smooth(0.24f, 0.3f, radius);
            const float sheen = 0.5f + 0.15f * smooth(0.1f, 0.26f, radius) + 0.05f * grain;
            over(texel, 0.16f, 0.16f, 0.17f, paint * 0.55f);
            over(texel, sheen, sheen, sheen * 1.02f, bare);
            over(texel, 0.01f, 0.01f, 0.01f, 1.0f - smooth(0.08f, 0.11f, point.radius));

            return texel;
        }

        Texel glass(const Point& point)
        {
            Texel texel{};
            const float crack = cracks(point, 11, 0.98f, 0.022f, 29u);
            const float ringNoise = angleNoise(point.angle, 10.0f, 31u);
            const float ringOne = (1.0f - smooth(0.006f, 0.016f, std::abs(point.radius - 0.42f - 0.05f * ringNoise)))
                * smooth(0.35f, 0.65f, angleNoise(point.angle, 7.0f, 37u));
            const float ringTwo = (1.0f - smooth(0.006f, 0.016f, std::abs(point.radius - 0.7f - 0.06f * ringNoise)))
                * smooth(0.45f, 0.75f, angleNoise(point.angle, 5.0f, 41u));
            const float crushed = (1.0f - smooth(0.1f, 0.2f, point.radius)) * (0.5f + 0.5f * hash(point.x, point.y));
            over(texel, 0.7f, 0.74f, 0.76f, std::max(crack, std::max(ringOne, ringTwo)) * 0.6f);
            over(texel, 0.62f, 0.65f, 0.68f, crushed * 0.75f);
            over(texel, 0.05f, 0.05f, 0.05f, 1.0f - smooth(0.03f, 0.055f, point.radius));

            return texel;
        }

        Texel fabric(const Point& point)
        {
            Texel texel{};
            const float frayed = 0.75f + 0.5f * angleNoise(point.angle, 24.0f, 43u);
            const float radius = point.radius / frayed;
            const float fibres = (1.0f - smooth(0.2f, 0.36f, radius)) * hash(point.x / 2u, point.y / 2u);
            over(texel, 0.25f, 0.24f, 0.22f, fibres * 0.5f);
            over(texel, 0.02f, 0.02f, 0.02f, 1.0f - smooth(0.12f, 0.2f, radius));

            return texel;
        }

        Texel paint(HoleSurface surface, const Point& point)
        {
            switch (surface)
            {
                case HoleSurface::Plaster:
                {
                    return plaster(point);
                }
                case HoleSurface::Tile:
                {
                    return tile(point);
                }
                case HoleSurface::Wood:
                {
                    return wood(point);
                }
                case HoleSurface::Metal:
                {
                    return metal(point);
                }
                case HoleSurface::Glass:
                {
                    return glass(point);
                }
                case HoleSurface::Fabric:
                {
                    return fabric(point);
                }
                case HoleSurface::Concrete:
                case HoleSurface::Count:
                {
                    return concrete(point);
                }
            }

            return concrete(point);
        }

        std::byte toByte(float value)
        {
            return static_cast<std::byte>(static_cast<uint8_t>(std::clamp(value, 0.0f, 1.0f) * 255.0f + 0.5f));
        }

        float toSrgb(float linear)
        {
            const float clamped = std::clamp(linear, 0.0f, 1.0f);
            return clamped <= 0.0031308f ? clamped * 12.92f : 1.055f * std::pow(clamped, 1.0f / 2.4f) - 0.055f;
        }
    } //namespace

    const char* holeSurfaceName(HoleSurface surface)
    {
        constexpr std::array<const char*, k_holeSurfaceCount> k_names{"concrete", "plaster", "tile", "wood", "metal",
            "glass", "fabric"};
        const auto index = static_cast<std::size_t>(surface);

        return index < k_names.size() ? k_names[index] : "concrete";
    }

    HoleStyle holeStyle(HoleSurface surface)
    {
        switch (surface)
        {
            case HoleSurface::Plaster:
            {
                return HoleStyle{.sizeScale = 1.35f, .relief = -0.04f, .roughness = 0.95f};
            }
            case HoleSurface::Tile:
            {
                return HoleStyle{.sizeScale = 1.8f, .relief = -0.015f, .roughness = 0.35f};
            }
            case HoleSurface::Wood:
            {
                return HoleStyle{.sizeScale = 1.3f, .relief = -0.025f, .roughness = 0.8f};
            }
            case HoleSurface::Metal:
            {
                return HoleStyle{.sizeScale = 0.75f, .relief = -0.05f, .roughness = 0.22f, .metallic = 1.0f};
            }
            case HoleSurface::Glass:
            {
                return HoleStyle{.sizeScale = 2.6f, .relief = 0.0f, .roughness = 0.05f};
            }
            case HoleSurface::Fabric:
            {
                return HoleStyle{.sizeScale = 0.6f, .relief = -0.01f, .roughness = 1.0f};
            }
            case HoleSurface::Concrete:
            case HoleSurface::Count:
            {
                return HoleStyle{};
            }
        }

        return HoleStyle{};
    }

    std::vector<std::byte> holeTexturePixels(HoleSurface surface)
    {
        std::vector<std::byte> pixels(static_cast<std::size_t>(k_holeTextureSize) * k_holeTextureSize * 4);
        const auto size = static_cast<float>(k_holeTextureSize);

        for (uint32_t y = 0; y < k_holeTextureSize; ++y)
        {
            for (uint32_t x = 0; x < k_holeTextureSize; ++x)
            {
                Point point{.u = (static_cast<float>(x) + 0.5f) / size * 2.0f - 1.0f,
                    .v = (static_cast<float>(y) + 0.5f) / size * 2.0f - 1.0f,
                    .x = x,
                    .y = y};
                point.radius = std::sqrt(point.u * point.u + point.v * point.v);
                point.angle = std::atan2(point.v, point.u);
                const Texel texel = paint(surface, point);
                const float edge = 1.0f - smooth(0.9f, 1.0f, point.radius);
                const std::size_t index = (static_cast<std::size_t>(y) * k_holeTextureSize + x) * 4;
                pixels[index] = toByte(toSrgb(texel.red));
                pixels[index + 1] = toByte(toSrgb(texel.green));
                pixels[index + 2] = toByte(toSrgb(texel.blue));
                pixels[index + 3] = toByte(texel.alpha * edge);
            }
        }

        return pixels;
    }
} //namespace game
