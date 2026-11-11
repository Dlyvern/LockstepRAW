#include "Lights/Screens.hpp"

#include <Lockstep/Assets/Scene.hpp>
#include <Lockstep/Core/Bounds.hpp>
#include <Lockstep/Render/PbrMaterial.hpp>
#include <Lockstep/Render/Renderer.hpp>

#if LOCKSTEP_DEV_TOOLS
#include <imgui.h>
#endif

#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <string_view>
#include <unordered_set>

namespace game
{
    namespace
    {
        namespace assets = lockstep::assets;
        namespace render = lockstep::render;

        constexpr std::string_view k_screenPrefix{"Screen_"};
        constexpr math::Vec3 k_screenFront{0.0f, 0.0f, 1.0f};
        constexpr float k_lightLift{0.04f};
        constexpr float k_innerAngle{0.7f};
        constexpr float k_outerAngle{1.4f};
        constexpr float k_volumetric{0.15f};
        constexpr float k_fadeBand{3.0f};
        constexpr float k_minArea{0.01f};
        constexpr float k_maxArea{2.0f};
        constexpr float k_maxScreenExtent{2.5f};
        constexpr float k_flickerSlot{0.07f};
        constexpr float k_shimmer{0.04f};

        struct ColorTag
        {
            std::string_view tag;
            math::Vec3 color;
        };

        constexpr std::array<ColorTag, 5> k_colorTags{{
            {"_Blue", {0.45f, 0.62f, 1.0f}},
            {"_Green", {0.45f, 1.0f, 0.6f}},
            {"_Amber", {1.0f, 0.68f, 0.3f}},
            {"_Red", {1.0f, 0.3f, 0.25f}},
            {"_White", {0.92f, 0.95f, 1.0f}}}};

        float hash(uint32_t seed, uint32_t slot)
        {
            uint32_t value = seed * 747796405u + slot * 2891336453u;
            value = (value ^ (value >> 16u)) * 2246822519u;
            value ^= value >> 13u;

            return static_cast<float>(value & 0xFFFFFFu) / static_cast<float>(0xFFFFFFu);
        }

        float distanceToBox(const math::Aabb& box, const math::Vec3& point)
        {
            const math::Vec3 nearest{std::clamp(point.x, box.min.x, box.max.x), std::clamp(point.y, box.min.y, box.max.y),
                std::clamp(point.z, box.min.z, box.max.z)};

            return math::length(point - nearest);
        }

        float faceArea(const math::Aabb& box, const math::Vec3& facing)
        {
            const math::Vec3 size = box.max - box.min;
            const float alongX = std::abs(facing.x);
            const float alongY = std::abs(facing.y);
            const float alongZ = std::abs(facing.z);

            return size.y * size.z * alongX + size.x * size.z * alongY + size.x * size.y * alongZ;
        }

        LevelScreen screenFromNode(const assets::Scene& scene, uint32_t node)
        {
            const assets::SceneNode& marker = scene.nodes[node];
            const std::string_view name = marker.name;
            LevelScreen screen{.position = marker.transform.position,
                .facing = math::normalize(marker.transform.rotation * k_screenFront),
                .behaviour = name.find("_Off") != std::string_view::npos ? ScreenBehaviour::Off
                    : name.find("_Flicker") != std::string_view::npos ? ScreenBehaviour::Flicker
                    : ScreenBehaviour::Steady,
                .scrolls = name.find("_Scroll") != std::string_view::npos,
                .isPanel = name.find("_Panel") != std::string_view::npos,
                .isImage = name.find("_Image") != std::string_view::npos,
                .seed = node * 2654435761u + 17u};

            for (const ColorTag& tag : k_colorTags)
            {
                if (name.find(tag.tag) != std::string_view::npos)
                {
                    screen.color = tag.color;
                }
            }

            const std::vector<uint32_t> owned = assets::subtreeInstances(scene, node);
            const std::unordered_set<uint32_t> ownedSet(owned.begin(), owned.end());
            float area = 0.0f;

            for (uint32_t index = 0; index < scene.instances.size(); ++index)
            {
                const assets::SceneInstance& instance = scene.instances[index];

                const math::Vec3 extent = instance.worldBounds.max - instance.worldBounds.min;

                if (!scene.meshes[instance.mesh].isEmissive
                    || std::max(extent.x, std::max(extent.y, extent.z)) > k_maxScreenExtent)
                {
                    continue;
                }

                if (ownedSet.contains(index) || distanceToBox(instance.worldBounds, screen.position) <= k_screenReach)
                {
                    screen.instances.push_back(index);
                    area = std::max(area, faceArea(instance.worldBounds, screen.facing));
                }
            }

            const math::Vec3 scale = marker.transform.scale;
            screen.area = std::clamp(area > 0.0f ? area : std::abs(scale.x * scale.y), k_minArea, k_maxArea);

            return screen;
        }

        void findScreens(LevelScreens& screens, const assets::Scene* scene)
        {
            screens.screens.clear();
            screens.scene = scene;
            screens.appliedEffects = -1.0f;

            if (scene == nullptr)
            {
                return;
            }

            for (uint32_t node = 0; node < scene->nodes.size(); ++node)
            {
                if (std::string_view(scene->nodes[node].name).starts_with(k_screenPrefix))
                {
                    screens.screens.push_back(screenFromNode(*scene, node));
                }
            }

            if (!screens.screens.empty())
            {
                std::clog << "Game: " << screens.screens.size() << " Screen_* markers\n";
            }
        }

        void applyMaterials(LevelScreens& screens, render::Renderer& renderer, const assets::Scene& scene)
        {
            const ScreenTuning& tuning = screens.tuning;

            const float effects = tuning.enabled ? tuning.effects : 0.0f;

            if (screens.appliedEffects == effects && screens.appliedScroll == tuning.scrollSpeed)
            {
                return;
            }

            for (const LevelScreen& screen : screens.screens)
            {
                if (screen.isPanel)
                {
                    continue;
                }

                for (const uint32_t instance : screen.instances)
                {
                    const render::MaterialHandle material = scene.meshes[scene.instances[instance].mesh].material;
                    static_cast<void>(renderer.setMaterialShading(material, render::MaterialShading{
                        .model = render::ShadingModel::Screen,
                        .strength = effects,
                        .scroll = screen.scrolls ? tuning.scrollSpeed : 0.0f,
                        .terminal = !screen.isImage}));
                }
            }

            screens.appliedEffects = effects;
            screens.appliedScroll = tuning.scrollSpeed;
        }

        float flickerLevel(const LevelScreen& screen, double seconds)
        {
            const auto slot = static_cast<uint32_t>(seconds / k_flickerSlot);
            const auto second = static_cast<uint32_t>(seconds);
            const float shimmer = 1.0f - k_shimmer * hash(screen.seed, slot);

            if (screen.behaviour == ScreenBehaviour::Off)
            {
                return 0.0f;
            }

            if (screen.behaviour == ScreenBehaviour::Steady)
            {
                return shimmer;
            }

            if (hash(screen.seed + 7u, second / 3u) < 0.12f)
            {
                return 0.03f;
            }

            const float dropout = hash(screen.seed + 3u, slot);

            if (dropout < 0.06f)
            {
                return 0.15f;
            }

            if (dropout < 0.12f)
            {
                return 0.55f;
            }

            return shimmer;
        }

        void submitLights(LevelScreens& screens, render::Renderer& renderer, const math::Vec3& eye)
        {
            const ScreenTuning& tuning = screens.tuning;
            std::vector<std::pair<float, const LevelScreen*>> ranked;

            for (const LevelScreen& screen : screens.screens)
            {
                const float distance = math::length(screen.position - eye);

                if (distance < tuning.reach && screen.level > 0.0f)
                {
                    ranked.emplace_back(distance, &screen);
                }
            }

            const std::size_t count = std::min<std::size_t>(ranked.size(), tuning.maxLights);
            std::partial_sort(ranked.begin(), ranked.begin() + static_cast<std::ptrdiff_t>(count), ranked.end(),
                [](const auto& left, const auto& right)
                {
                    return left.first < right.first;
                });

            for (std::size_t index = 0; index < count; ++index)
            {
                const auto& [distance, screen] = ranked[index];
                const float fade = std::clamp((tuning.reach - distance) / k_fadeBand, 0.0f, 1.0f);
                renderer.submitLight(render::SpotLight{.position = screen->position + screen->facing * k_lightLift,
                    .direction = screen->facing,
                    .color = screen->color,
                    .intensity = tuning.lightIntensity * screen->area * screen->level * fade,
                    .range = tuning.lightRange,
                    .innerAngle = k_innerAngle,
                    .outerAngle = k_outerAngle,
                    .volumetric = k_volumetric});
                ++screens.lit;
            }
        }
    } //namespace

    uint32_t presentScreens(LevelScreens& screens, render::Renderer& renderer, assets::Scene* scene,
        const math::Vec3& eye, float worldSeconds)
    {
        screens.lit = 0;
        render::RenderSettings& settings = renderer.settings();
        settings.surfaceEffects.time = std::fmod(settings.surfaceEffects.time + std::max(worldSeconds, 0.0f), 3600.0f);

        if (scene != screens.scene)
        {
            findScreens(screens, scene);
        }

        if (scene == nullptr || screens.screens.empty())
        {
            return 0;
        }

        applyMaterials(screens, renderer, *scene);
        screens.seconds += static_cast<double>(std::max(worldSeconds, 0.0f));

        for (LevelScreen& screen : screens.screens)
        {
            screen.level = screens.tuning.enabled ? flickerLevel(screen, screens.seconds) : 1.0f;

            for (const uint32_t instance : screen.instances)
            {
                const float brightness = screen.isPanel ? 1.0f : screens.tuning.brightness;
                scene->instances[instance].emissiveScale = screen.level * brightness;
            }
        }

        if (screens.tuning.enabled && screens.tuning.lights)
        {
            submitLights(screens, renderer, eye);
        }

        return screens.lit;
    }

#if LOCKSTEP_DEV_TOOLS
    void screensDevSection(LevelScreens& screens)
    {
        if (!ImGui::TreeNode("Screens"))
        {
            return;
        }

        ScreenTuning& tuning = screens.tuning;
        ImGui::Checkbox("Animate screens", &tuning.enabled);
        ImGui::Checkbox("Screen lights", &tuning.lights);
        ImGui::Text("%zu screens, %u lit (budget %u from graphics quality)", screens.screens.size(), screens.lit,
            tuning.maxLights);
        ImGui::SliderFloat("Screen brightness", &tuning.brightness, 0.0f, 2.0f, "%.2f");
        ImGui::SliderFloat("Light per m2", &tuning.lightIntensity, 0.0f, 3.0f, "%.2f");
        ImGui::SliderFloat("Light range (m)", &tuning.lightRange, 0.5f, 8.0f, "%.1f");
        ImGui::SliderFloat("Reach (m)", &tuning.reach, 2.0f, 40.0f, "%.0f");
        ImGui::SliderFloat("Scanlines and band", &tuning.effects, 0.0f, 2.0f, "%.2f");
        ImGui::SliderFloat("Scroll speed", &tuning.scrollSpeed, 0.0f, 0.5f, "%.3f");
        ImGui::TreePop();
    }
#endif
} //namespace game
