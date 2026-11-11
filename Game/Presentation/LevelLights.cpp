#include "Presentation/LevelLights.hpp"

#include <Lockstep/Assets/SceneLights.hpp>
#include <Lockstep/Render/Lighting.hpp>
#include <Lockstep/Render/Renderer.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <string>
#include <string_view>
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
        constexpr std::string_view k_shadowNameTag{"_Shadow"};
        constexpr std::string_view k_tubeNameTag{"_Tube"};
        constexpr math::Vec3 k_lightSide{1.0f, 0.0f, 0.0f};
        constexpr math::Vec3 k_lightForward{0.0f, 0.0f, -1.0f};

        struct ProfileTag
        {
            std::string_view tag;
            render::LightProfile profile{render::LightProfile::None};
        };

        constexpr ProfileTag k_profileTags[]{
            {"_Downlight", render::LightProfile::Downlight},
            {"_WallWash", render::LightProfile::WallWash},
            {"_Grille", render::LightProfile::Grille},
            {"_Fluorescent", render::LightProfile::Fluorescent}};

        render::LightProfile profileFromName(std::string_view name, bool isTube)
        {
            for (const ProfileTag& entry : k_profileTags)
            {
                if (name.find(entry.tag) != std::string_view::npos)
                {
                    return entry.profile;
                }
            }

            return isTube ? render::LightProfile::Fluorescent : render::LightProfile::None;
        }

        void applyShape(render::PointLight& point, const assets::SceneLight& light)
        {
            const bool isTube = light.name.find(k_tubeNameTag) != std::string::npos;
            const math::Vec3 side = math::normalize(light.transform.rotation * k_lightSide);

            if (isTube)
            {
                point.tubeLength = std::abs(light.transform.scale.x);
                point.tubeAxis = side;
            }

            point.profile = profileFromName(light.name, isTube);
            point.profileAxis = math::normalize(light.transform.rotation * k_lightForward);
            point.profileSide = side;
        }

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

    void submitLevelLights(render::Renderer& renderer, const assets::Scene& scene, const math::Vec3& eye,
        std::span<const LightOverride> overrides, std::size_t maxLights)
    {
        const std::vector<RankedLight> ranked = rankLights(scene, eye);
        const std::size_t budget = std::min<std::size_t>(maxLights, render::k_maxLocalLights);
        const float cutoff = ranked.size() > budget ? std::min(k_reach, ranked[budget].edgeDistance) : k_reach;
        const std::size_t submitted = std::min<std::size_t>(ranked.size(), budget);

        for (std::size_t index = 0; index < submitted; ++index)
        {
            const RankedLight& entry = ranked[index];
            const float fade = std::clamp((cutoff - entry.edgeDistance) / k_fadeBand, 0.0f, 1.0f);

            if (fade <= 0.0f)
            {
                continue;
            }

            const bool castsShadows = entry.light->name.find(k_shadowNameTag) != std::string::npos;
            const auto lightIndex = static_cast<std::size_t>(entry.light - scene.lights.data());
            const LightOverride change = lightIndex < overrides.size() ? overrides[lightIndex] : LightOverride{};
            const float intensity = fade * change.intensity;

            if (intensity <= 0.0f && !castsShadows)
            {
                continue;
            }

            if (entry.light->type == assets::SceneLightType::Spot)
            {
                render::SpotLight spot = assets::toSpotLight(*entry.light);
                spot.intensity *= intensity;
                spot.color = change.color.value_or(spot.color);
                spot.castsShadows = castsShadows;
                spot.profile = profileFromName(entry.light->name, false);
                spot.profileSide = math::normalize(entry.light->transform.rotation * k_lightSide);
                renderer.submitLight(spot);
            }
            else
            {
                render::PointLight point = assets::toPointLight(*entry.light);
                point.intensity *= intensity;
                point.color = change.color.value_or(point.color);
                point.castsShadows = castsShadows;
                applyShape(point, *entry.light);
                renderer.submitLight(point);
            }
        }
    }
} //namespace game
