#include "Lockstep/Assets/SceneLights.hpp"

#include <algorithm>
#include <cmath>

namespace lockstep::assets
{
    namespace
    {
        constexpr float k_minDerivedRange{0.5f};
        constexpr float k_maxDerivedRange{50.0f};
        constexpr float k_minConeGap{0.01f};
        const math::Vec3 k_lightForward{0.0f, 0.0f, -1.0f};
    } //namespace

    float renderIntensity(const SceneLight& light)
    {
        return std::max(light.intensity, 0.0f) / k_candelaPerRenderIntensity;
    }

    float renderRange(const SceneLight& light)
    {
        if (light.range > 0.0f)
        {
            return light.range;
        }

        const float brightest = std::max({light.color.x, light.color.y, light.color.z, 0.0f}) * renderIntensity(light);

        return std::clamp(std::sqrt(brightest / k_derivedRangeCutoff), k_minDerivedRange, k_maxDerivedRange);
    }

    render::PointLight toPointLight(const SceneLight& light)
    {
        return render::PointLight{.position = light.transform.position,
            .color = light.color,
            .intensity = renderIntensity(light),
            .range = renderRange(light)};
    }

    render::SpotLight toSpotLight(const SceneLight& light)
    {
        const float outerAngle = std::max(light.outerAngle, k_minConeGap);

        return render::SpotLight{.position = light.transform.position,
            .direction = math::normalize(light.transform.rotation * k_lightForward),
            .color = light.color,
            .intensity = renderIntensity(light),
            .range = renderRange(light),
            .innerAngle = std::clamp(light.innerAngle, 0.0f, outerAngle - k_minConeGap),
            .outerAngle = outerAngle,
            .castsShadows = false};
    }
} //namespace lockstep::assets
