#include "SkyModel.hpp"

#include <algorithm>
#include <cmath>

namespace lockstep::render
{
    namespace
    {
        constexpr float k_luminanceToRadiance{0.06f};
        constexpr float k_maxSunZenith{math::radians(89.0f)};
        constexpr float k_nightFloor{0.02f};
        constexpr float k_maxAirMass{38.0f};
        constexpr math::Vec3 k_rayleighDepth{0.0423f, 0.1002f, 0.2489f};
        constexpr math::Vec3 k_aerosolWavelengthFactor{1.65f, 2.17f, 2.91f};

        struct Perez
        {
            float a;
            float b;
            float c;
            float d;
            float e;
        };

        float perezAt(const Perez& coefficients, float cosTheta, float gamma)
        {
            const float cosGamma = std::cos(gamma);

            return (1.0f + coefficients.a * std::exp(coefficients.b / std::max(cosTheta, 0.01f)))
                * (1.0f + coefficients.c * std::exp(coefficients.d * gamma) + coefficients.e * cosGamma * cosGamma);
        }

        Perez luminanceCoefficients(float turbidity)
        {
            return {0.1787f * turbidity - 1.4630f, -0.3554f * turbidity + 0.4275f, -0.0227f * turbidity + 5.3251f,
                0.1206f * turbidity - 2.5771f, -0.0670f * turbidity + 0.3703f};
        }

        Perez chromaticityXCoefficients(float turbidity)
        {
            return {-0.0193f * turbidity - 0.2592f, -0.0665f * turbidity + 0.0008f, -0.0004f * turbidity + 0.2125f,
                -0.0641f * turbidity - 0.8989f, -0.0033f * turbidity + 0.0452f};
        }

        Perez chromaticityYCoefficients(float turbidity)
        {
            return {-0.0167f * turbidity - 0.2608f, -0.0950f * turbidity + 0.0092f, -0.0079f * turbidity + 0.2102f,
                -0.0441f * turbidity - 1.6537f, -0.0109f * turbidity + 0.0529f};
        }

        math::Vec3 zenithYxy(float turbidity, float sunZenith)
        {
            const float chi = (4.0f / 9.0f - turbidity / 120.0f) * (math::k_pi - 2.0f * sunZenith);
            const float luminance = (4.0453f * turbidity - 4.9710f) * std::tan(chi) - 0.2155f * turbidity + 2.4192f;

            const float t2 = turbidity * turbidity;
            const float s = sunZenith;
            const float s2 = s * s;
            const float s3 = s2 * s;

            const float x = t2 * (0.00166f * s3 - 0.00375f * s2 + 0.00209f * s)
                + turbidity * (-0.02903f * s3 + 0.06377f * s2 - 0.03202f * s + 0.00394f)
                + (0.11693f * s3 - 0.21196f * s2 + 0.06052f * s + 0.25886f);
            const float y = t2 * (0.00275f * s3 - 0.00610f * s2 + 0.00317f * s)
                + turbidity * (-0.04214f * s3 + 0.08970f * s2 - 0.04153f * s + 0.00516f)
                + (0.15346f * s3 - 0.26756f * s2 + 0.06670f * s + 0.26688f);

            return {std::max(luminance, 0.0f), x, y};
        }

        math::Vec3 xyYToLinearRgb(const math::Vec3& yxy)
        {
            const float luminance = yxy.x;
            const float y = std::max(yxy.z, 1e-4f);
            const float bigX = yxy.y / y * luminance;
            const float bigZ = (1.0f - yxy.y - y) / y * luminance;

            return {3.2406f * bigX - 1.5372f * luminance - 0.4986f * bigZ,
                -0.9689f * bigX + 1.8758f * luminance + 0.0415f * bigZ,
                0.0557f * bigX - 0.2040f * luminance + 1.0570f * bigZ};
        }

        float airMass(float zenithAngle)
        {
            const float degrees = math::degrees(zenithAngle);

            if (degrees >= 90.0f)
            {
                return k_maxAirMass;
            }

            const float mass = 1.0f / (std::cos(zenithAngle) + 0.50572f * std::pow(96.07995f - degrees, -1.6364f));

            return std::min(mass, k_maxAirMass);
        }

        float smoothstep(float edge0, float edge1, float value)
        {
            const float t = std::clamp((value - edge0) / (edge1 - edge0), 0.0f, 1.0f);
            return t * t * (3.0f - 2.0f * t);
        }

        math::Vec3 componentwise(const Perez& luminance, const Perez& x, const Perez& y, float Perez::*member)
        {
            return {luminance.*member, x.*member, y.*member};
        }
    } //namespace

    math::Vec3 sunTravelDirection(float azimuth, float elevation)
    {
        const math::Vec3 toSun{std::sin(azimuth) * std::cos(elevation), std::sin(elevation),
            -std::cos(azimuth) * std::cos(elevation)};

        return toSun * -1.0f;
    }

    math::Vec3 skySunRadiance(const SkySettings& sky)
    {
        const math::Vec3 radiance = sky.sunColor * sky.sunIntensity;
        const float horizonFade = smoothstep(-0.03f, 0.03f, std::sin(sky.sunElevation));

        if (!sky.atmosphericSunTint)
        {
            return radiance * horizonFade;
        }

        const float turbidity = std::max(sky.turbidity, 1.0f);
        const float aerosol = std::max(0.04608f * turbidity - 0.04586f, 0.0f);
        const math::Vec3 depth = k_rayleighDepth + k_aerosolWavelengthFactor * aerosol;
        const float extraMass = airMass(math::k_pi * 0.5f - sky.sunElevation) - 1.0f;
        const math::Vec3 transmittance{std::exp(-depth.x * extraMass), std::exp(-depth.y * extraMass),
            std::exp(-depth.z * extraMass)};

        return math::Vec3{radiance.x * transmittance.x, radiance.y * transmittance.y, radiance.z * transmittance.z}
            * horizonFade;
    }

    SkyCoefficients computeSky(const SkySettings& sky, const math::Vec3& toSun, const math::Vec3& sunRadiance)
    {
        const float turbidity = std::clamp(sky.turbidity, 1.7f, 10.0f);
        const float sunHeight = std::clamp(toSun.y, -1.0f, 1.0f);
        const float sunZenith = std::min(std::acos(sunHeight), k_maxSunZenith);

        const Perez luminance = luminanceCoefficients(turbidity);
        const Perez chromaX = chromaticityXCoefficients(turbidity);
        const Perez chromaY = chromaticityYCoefficients(turbidity);
        const math::Vec3 zenith = zenithYxy(turbidity, sunZenith);

        SkyCoefficients result{};
        result.perezA = componentwise(luminance, chromaX, chromaY, &Perez::a);
        result.perezB = componentwise(luminance, chromaX, chromaY, &Perez::b);
        result.perezC = componentwise(luminance, chromaX, chromaY, &Perez::c);
        result.perezD = componentwise(luminance, chromaX, chromaY, &Perez::d);
        result.perezE = componentwise(luminance, chromaX, chromaY, &Perez::e);
        result.zenith = {zenith.x / perezAt(luminance, 1.0f, sunZenith), zenith.y / perezAt(chromaX, 1.0f, sunZenith),
            zenith.z / perezAt(chromaY, 1.0f, sunZenith)};

        const float night = std::max(smoothstep(-0.2f, 0.05f, sunHeight), k_nightFloor);
        result.radianceScale = k_luminanceToRadiance * sky.skyIntensity * night;
        result.glow = sky.glowColor * (std::max(sky.glowIntensity, 0.0f) * sky.skyIntensity * night);

        const math::Vec3 zenithRadiance = math::max(xyYToLinearRgb(zenith), math::Vec3{0.0f, 0.0f, 0.0f})
            * result.radianceScale;
        const math::Vec3 groundIrradiance = sunRadiance * std::max(sunHeight, 0.0f) + zenithRadiance;
        result.ground = {sky.groundAlbedo.x * groundIrradiance.x, sky.groundAlbedo.y * groundIrradiance.y,
            sky.groundAlbedo.z * groundIrradiance.z};

        return result;
    }
} //namespace lockstep::render
