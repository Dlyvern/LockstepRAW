#include "LightProfiles.hpp"

#include "Lockstep/Render/Lighting.hpp"

#include <algorithm>
#include <cmath>

namespace lockstep::render
{
    namespace
    {
        constexpr float k_pi{3.14159265358979f};
        constexpr float k_degree{k_pi / 180.0f};

        float smoothStep(float edge0, float edge1, float value)
        {
            const float t = std::clamp((value - edge0) / (edge1 - edge0), 0.0f, 1.0f);

            return t * t * (3.0f - 2.0f * t);
        }

        float downlight(float theta, float)
        {
            const float core = smoothStep(58.0f * k_degree, 32.0f * k_degree, theta);
            const float spill = 0.1f * smoothStep(100.0f * k_degree, 50.0f * k_degree, theta);

            return std::max(core, spill);
        }

        float wallWash(float theta, float phi)
        {
            const float towardsWall = std::pow(std::max(std::cos(phi), 0.0f), 1.5f);
            const float tilt = 0.5f * towardsWall;
            const float reach = smoothStep((95.0f - 10.0f * (1.0f - towardsWall)) * k_degree, 40.0f * k_degree,
                theta - tilt * 0.2f);

            return reach * (0.2f + 0.8f * towardsWall);
        }

        float grille(float theta, float phi)
        {
            const float base = smoothStep(105.0f * k_degree, 70.0f * k_degree, theta);
            const float spread = std::tan(std::min(theta, 80.0f * k_degree));
            const float acrossBars = spread * std::sin(phi);
            const float bars = 0.3f + 0.7f * smoothStep(0.15f, 0.45f, std::abs(std::sin(acrossBars * 9.0f)));
            const float rings = 0.55f + 0.45f * smoothStep(0.1f, 0.5f, std::abs(std::sin(spread * 4.0f)));

            return base * bars * (theta < 12.0f * k_degree ? 1.0f : rings);
        }

        float fluorescent(float theta, float phi)
        {
            const float across = std::abs(std::sin(phi));
            const float batwing = 0.75f + 0.25f * std::sin(std::min(theta, 90.0f * k_degree) * 2.0f);
            const float edge = smoothStep((88.0f + 10.0f * across) * k_degree, (55.0f + 15.0f * across) * k_degree, theta);

            return std::max(batwing * edge, 0.04f * smoothStep(180.0f * k_degree, 90.0f * k_degree, theta));
        }

        float profileValue(LightProfile profile, float theta, float phi)
        {
            switch (profile)
            {
                case LightProfile::Downlight:
                {
                    return downlight(theta, phi);
                }
                case LightProfile::WallWash:
                {
                    return wallWash(theta, phi);
                }
                case LightProfile::Grille:
                {
                    return grille(theta, phi);
                }
                case LightProfile::Fluorescent:
                {
                    return fluorescent(theta, phi);
                }
                case LightProfile::None:
                {
                    break;
                }
            }

            return 1.0f;
        }
    } //namespace

    std::vector<uint8_t> lightProfileTexels()
    {
        std::vector<uint8_t> texels(static_cast<std::size_t>(k_lightProfileWidth) * k_lightProfileHeight
            * k_lightProfileCount);

        for (uint32_t index = 0; index < k_lightProfileCount; ++index)
        {
            const auto profile = static_cast<LightProfile>(index + 1);
            std::vector<float> values(static_cast<std::size_t>(k_lightProfileWidth) * k_lightProfileHeight);
            float peak = 1e-4f;

            for (uint32_t row = 0; row < k_lightProfileHeight; ++row)
            {
                const float cosTheta = 1.0f - 2.0f * (static_cast<float>(row) + 0.5f) / static_cast<float>(k_lightProfileHeight);
                const float theta = std::acos(std::clamp(cosTheta, -1.0f, 1.0f));

                for (uint32_t column = 0; column < k_lightProfileWidth; ++column)
                {
                    const float phi = ((static_cast<float>(column) + 0.5f) / static_cast<float>(k_lightProfileWidth)
                        - 0.5f) * 2.0f * k_pi;
                    const float value = profileValue(profile, theta, phi);
                    values[row * k_lightProfileWidth + column] = value;
                    peak = std::max(peak, value);
                }
            }

            const std::size_t offset = static_cast<std::size_t>(index) * k_lightProfileWidth * k_lightProfileHeight;

            for (std::size_t texel = 0; texel < values.size(); ++texel)
            {
                texels[offset + texel] = static_cast<uint8_t>(std::lround(std::clamp(values[texel] / peak, 0.0f, 1.0f)
                    * 255.0f));
            }
        }

        return texels;
    }
} //namespace lockstep::render
