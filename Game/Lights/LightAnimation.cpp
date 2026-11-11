#include "Lights/LightAnimation.hpp"

#include <Lockstep/App/GameApi.hpp>
#include <Lockstep/Assets/Scene.hpp>
#include <Lockstep/Render/Renderer.hpp>

#if LOCKSTEP_DEV_TOOLS
#include <imgui.h>
#endif

#include <algorithm>
#include <array>
#include <optional>

namespace game
{
    namespace
    {
        namespace assets = lockstep::assets;

        constexpr float k_carRadius{3.0f};
        constexpr float k_litLevel{0.5f};

        bool isSiren(LightPattern pattern)
        {
            return pattern == LightPattern::SirenRed || pattern == LightPattern::SirenBlue;
        }

        bool isTube(LightPattern pattern)
        {
            return pattern == LightPattern::Flicker || pattern == LightPattern::FlickerHard;
        }

        LightPattern testPatternFor(LightPattern testPattern, std::size_t testCount)
        {
            if (isSiren(testPattern))
            {
                return testCount % 2 == 0 ? LightPattern::SirenRed : LightPattern::SirenBlue;
            }

            return testPattern;
        }

        void shareCarSeeds(std::vector<AnimatedLight>& lights, const assets::Scene& scene)
        {
            for (std::size_t current = 0; current < lights.size(); ++current)
            {
                if (!isSiren(lights[current].pattern))
                {
                    continue;
                }

                const math::Vec3 position = scene.lights[lights[current].index].transform.position;

                for (std::size_t earlier = 0; earlier < current; ++earlier)
                {
                    const math::Vec3 other = scene.lights[lights[earlier].index].transform.position;

                    if (isSiren(lights[earlier].pattern) && math::length(position - other) < k_carRadius)
                    {
                        lights[current].seed = lights[earlier].seed;
                        break;
                    }
                }
            }
        }

        void buildLights(LightAnimation& animation, const assets::Scene& scene)
        {
            const LightPattern testPattern = animation.settings.testPattern;
            animation.lights.clear();
            std::size_t testCount = 0;

            for (std::size_t index = 0; index < scene.lights.size(); ++index)
            {
                const assets::SceneLight& light = scene.lights[index];

                if (light.type == assets::SceneLightType::Directional)
                {
                    continue;
                }

                LightPattern pattern = lightPatternFromName(light.name);

                if (pattern == LightPattern::None && testPattern != LightPattern::None)
                {
                    pattern = testPatternFor(testPattern, testCount++);
                }

                if (pattern != LightPattern::None)
                {
                    const auto lightIndex = static_cast<uint32_t>(index);
                    animation.lights.push_back(
                        {.index = lightIndex, .pattern = pattern, .seed = lightSeed(light.name, lightIndex)});
                }
            }

            shareCarSeeds(animation.lights, scene);
            animation.overrides.assign(scene.lights.size(), LightOverride{});
            animation.scene = &scene;
            animation.sceneLightCount = scene.lights.size();
            animation.builtTestPattern = testPattern;
        }

        float distanceToBounds(const math::Vec3& point, const math::Aabb& bounds)
        {
            const math::Vec3 nearest{std::clamp(point.x, bounds.min.x, bounds.max.x),
                std::clamp(point.y, bounds.min.y, bounds.max.y), std::clamp(point.z, bounds.min.z, bounds.max.z)};
            return math::length(point - nearest);
        }

        std::optional<uint32_t> nearestLight(const assets::Scene& scene, const math::Aabb& bounds)
        {
            std::optional<uint32_t> nearest;
            float nearestDistance = k_lampReach;

            for (std::size_t index = 0; index < scene.lights.size(); ++index)
            {
                const assets::SceneLight& light = scene.lights[index];

                if (light.type == assets::SceneLightType::Directional)
                {
                    continue;
                }

                const float distance = distanceToBounds(light.transform.position, bounds);

                if (distance <= nearestDistance)
                {
                    nearestDistance = distance;
                    nearest = static_cast<uint32_t>(index);
                }
            }

            return nearest;
        }

        void linkLamps(LightAnimation& animation, const assets::Scene& scene)
        {
            animation.lamps.clear();

            for (std::size_t instance = 0; instance < scene.instances.size(); ++instance)
            {
                const assets::SceneInstance& placed = scene.instances[instance];

                if (!scene.meshes[placed.mesh].isEmissive)
                {
                    continue;
                }

                const std::optional<uint32_t> light = nearestLight(scene, placed.worldBounds);

                if (!light)
                {
                    continue;
                }

                for (std::size_t animated = 0; animated < animation.lights.size(); ++animated)
                {
                    if (animation.lights[animated].index == *light)
                    {
                        animation.lamps.push_back(
                            {.instance = static_cast<uint32_t>(instance), .light = static_cast<uint32_t>(animated)});
                        break;
                    }
                }
            }
        }

        void setLampLevels(const LightAnimation& animation, assets::Scene& scene, bool isAnimated)
        {
            for (const LampLink& lamp : animation.lamps)
            {
                scene.instances[lamp.instance].emissiveScale =
                    isAnimated && animation.settings.dimLamps ? animation.lights[lamp.light].level : 1.0f;
            }
        }

        bool needsRebuild(const LightAnimation& animation, const assets::Scene& scene)
        {
            return animation.scene != &scene || animation.sceneLightCount != scene.lights.size()
                || animation.builtTestPattern != animation.settings.testPattern;
        }

        std::vector<BuzzSource> animateLights(LightAnimation& animation, const assets::Scene& scene)
        {
            std::vector<BuzzSource> tubes;

            for (AnimatedLight& light : animation.lights)
            {
                const float previous = light.level;
                light.level = lightPatternLevel(light.pattern, light.seed, animation.seconds,
                    animation.settings.patterns);
                animation.overrides[light.index] =
                    LightOverride{.intensity = light.level, .color = lightPatternColor(light.pattern)};

                if (isTube(light.pattern))
                {
                    tubes.push_back({.key = light.index,
                        .position = scene.lights[light.index].transform.position,
                        .level = light.level,
                        .toggled = (previous >= k_litLevel) != (light.level >= k_litLevel)});
                }
            }

            return tubes;
        }
    } //namespace

    void presentLevelLights(LightAnimation& animation, lockstep::app::EngineContext& engine, assets::Scene* scene,
        const math::Vec3& eye, float worldSeconds)
    {
        if (!scene)
        {
            stopLightBuzz(animation.buzz, engine.audio);
            animation.scene = nullptr;
            animation.lights.clear();
            animation.lamps.clear();
            return;
        }

        loadLightBuzz(animation.buzz, engine.audio, engine.files);
        animation.screens.tuning.maxLights = engine.renderer.graphicsQuality().screenLights;
        const uint32_t screenLights = presentScreens(animation.screens, engine.renderer, scene, eye, worldSeconds);
        const std::size_t levelLights = lockstep::render::k_maxLocalLights - std::min(screenLights,
            lockstep::render::k_maxLocalLights);

        if (needsRebuild(animation, *scene))
        {
            stopLightBuzz(animation.buzz, engine.audio);
            buildLights(animation, *scene);
            linkLamps(animation, *scene);
        }

        if (!animation.settings.isEnabled)
        {
            stopLightBuzz(animation.buzz, engine.audio);
            setLampLevels(animation, *scene, false);
            submitLevelLights(engine.renderer, *scene, eye, {}, levelLights);
            return;
        }

        animation.seconds += static_cast<double>(worldSeconds);
        const std::vector<BuzzSource> tubes = animateLights(animation, *scene);
        setLampLevels(animation, *scene, true);
        submitLevelLights(engine.renderer, *scene, eye, animation.overrides, levelLights);
        updateLightBuzz(animation.buzz, engine.audio, tubes, eye, animation.seconds);
    }

#if LOCKSTEP_DEV_TOOLS
    void lightAnimationDevSection(LightAnimation& animation)
    {
        if (!ImGui::TreeNode("Light animation"))
        {
            return;
        }

        screensDevSection(animation.screens);
        LightAnimationSettings& settings = animation.settings;
        ImGui::Checkbox("Animate lights", &settings.isEnabled);
        ImGui::Checkbox("Dim lamp meshes with their light", &settings.dimLamps);
        ImGui::Text("%zu lamp meshes follow animated lights", animation.lamps.size());

        std::array<int, 6> counts{};

        for (const AnimatedLight& light : animation.lights)
        {
            ++counts[static_cast<std::size_t>(light.pattern)];
        }

        ImGui::Text("Flicker %d, hard %d, siren red %d / blue %d, pulse %d", counts[1], counts[2], counts[3],
            counts[4], counts[5]);

        constexpr std::array<LightPattern, 5> k_testPatterns{LightPattern::None, LightPattern::Flicker,
            LightPattern::FlickerHard, LightPattern::SirenRed, LightPattern::Pulse};

        if (ImGui::BeginCombo("Test on all lights", lightPatternName(settings.testPattern)))
        {
            for (const LightPattern pattern : k_testPatterns)
            {
                if (ImGui::Selectable(isSiren(pattern) ? "Siren" : lightPatternName(pattern),
                        pattern == settings.testPattern))
                {
                    settings.testPattern = pattern;
                }
            }

            ImGui::EndCombo();
        }

        LightPatternTuning& tuning = settings.patterns;
        ImGui::SliderFloat("Flicker episode (s)", &tuning.flickerWindow, 0.5f, 10.0f);
        ImGui::SliderFloat("Flicker step (s)", &tuning.flickerSlot, 0.01f, 0.2f);
        ImGui::SliderFloat("Dropout chance", &tuning.flickerDropout, 0.0f, 0.05f, "%.4f");
        ImGui::SliderFloat("Burst chance", &tuning.flickerBurstChance, 0.0f, 1.0f);
        ImGui::SliderFloat("Long off chance", &tuning.flickerOffChance, 0.0f, 1.0f);
        ImGui::SliderFloat("Off glow", &tuning.flickerOffLevel, 0.0f, 0.5f);
        ImGui::SliderFloat("Siren cycle (s)", &tuning.sirenCycle, 0.2f, 2.0f);
        ImGui::SliderFloat("Siren flash (s)", &tuning.sirenFlash, 0.02f, 0.2f);
        ImGui::SliderFloat("Siren gap (s)", &tuning.sirenGap, 0.0f, 0.2f);
        ImGui::SliderFloat("Siren rate jitter", &tuning.sirenRateJitter, 0.0f, 0.3f);
        ImGui::SliderFloat("Pulse period (s)", &tuning.pulsePeriod, 0.3f, 8.0f);
        ImGui::SliderFloat("Pulse low", &tuning.pulseLow, 0.0f, 1.0f);
        lightBuzzDevSection(animation.buzz);
        ImGui::TreePop();
    }
#endif
} //namespace game
