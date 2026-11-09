#include "Presentation/LevelLights.hpp"

#include <Lockstep/Assets/SceneLights.hpp>
#include <Lockstep/Render/Lighting.hpp>
#include <Lockstep/Render/Renderer.hpp>

#include <algorithm>
#include <cstddef>
#include <vector>

namespace game
{
    namespace
    {
        namespace assets = lockstep::assets;
        namespace render = lockstep::render;
        namespace math = lockstep::math;

        constexpr float k_reach{12.0f};
        constexpr float k_fadeBand{3.0f};

        struct RankedLight
        {
            const assets::SceneLight* light{nullptr};
            float range{0.0f};
            float edgeDistance{0.0f};
        };

        std::vector<RankedLight> rankLights(const assets::Scene& scene, const math::Vec3& eye)
        {
            std::vector<RankedLight> ranked;
            ranked.reserve(scene.lights.size());

            for (const assets::SceneLight& light : scene.lights)
            {
                if (light.type == assets::SceneLightType::Directional || !(light.intensity > 0.0f))
                {
                    continue;
                }

                const float range = assets::renderRange(light);
                const float edgeDistance = math::length(light.transform.position - eye) - range;

                if (edgeDistance < k_reach)
                {
                    ranked.push_back({.light = &light, .range = range, .edgeDistance = edgeDistance});
                }
            }

            const std::size_t kept = std::min<std::size_t>(ranked.size(), render::k_maxLocalLights + 1);
            std::partial_sort(ranked.begin(), ranked.begin() + static_cast<std::ptrdiff_t>(kept), ranked.end(),
                [](const RankedLight& left, const RankedLight& right)
                {
                    return left.edgeDistance < right.edgeDistance;
                });
            ranked.resize(kept);

            return ranked;
        }
    } //namespace

    void submitLevelLights(render::Renderer& renderer, const assets::Scene& scene, const math::Vec3& eye)
    {
        const std::vector<RankedLight> ranked = rankLights(scene, eye);
        const float cutoff = ranked.size() > render::k_maxLocalLights
            ? std::min(k_reach, ranked[render::k_maxLocalLights].edgeDistance)
            : k_reach;
        const std::size_t submitted = std::min<std::size_t>(ranked.size(), render::k_maxLocalLights);

        for (std::size_t index = 0; index < submitted; ++index)
        {
            const RankedLight& entry = ranked[index];
            const float fade = std::clamp((cutoff - entry.edgeDistance) / k_fadeBand, 0.0f, 1.0f);

            if (fade <= 0.0f)
            {
                continue;
            }

            if (entry.light->type == assets::SceneLightType::Spot)
            {
                render::SpotLight spot = assets::toSpotLight(*entry.light);
                spot.intensity *= fade;
                renderer.submitLight(spot);
            }
            else
            {
                render::PointLight point = assets::toPointLight(*entry.light);
                point.intensity *= fade;
                renderer.submitLight(point);
            }
        }
    }
} //namespace game
