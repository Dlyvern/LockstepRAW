#include "Presentation/MaterialLooks.hpp"

#include <Lockstep/Assets/Scene.hpp>
#include <Lockstep/Render/Renderer.hpp>

#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>
#include <iostream>
#include <string>
#include <string_view>

namespace game
{
    namespace
    {
        namespace render = lockstep::render;
        namespace assets = lockstep::assets;

        constexpr std::array<std::string_view, 4> k_skinWords{"face", "skin", "hand", "head"};
        constexpr std::array<std::string_view, 6> k_clothingWords{"body", "robber", "tac_set", "uniform", "cloth",
            "fabric"};
        constexpr std::array<std::string_view, 16> k_furnishingWords{"mattress", "sofa", "couch", "curtain", "drape",
            "pillow", "blanket", "cushion", "fabric", "cloth", "carpet", "rug", "towel", "uniform", "linen", "trashbag"};
        constexpr std::string_view k_notSkinWord{"headgear"};
        constexpr std::string_view k_notFabricWord{"frame"};
        constexpr uint32_t k_ancestorsChecked{2};
        constexpr std::string_view k_timingDrillFlag{"--dev-material-ab"};

        constexpr render::MaterialShading k_skin{.model = render::ShadingModel::Skin, .strength = 1.0f};
        constexpr render::MaterialShading k_clothing{.model = render::ShadingModel::Fabric, .strength = 0.35f,
            .sheenRoughness = 0.75f};
        constexpr render::MaterialShading k_furnishing{.model = render::ShadingModel::Fabric, .strength = 1.0f,
            .sheenRoughness = 0.6f};

        std::string lowered(std::string_view text)
        {
            std::string result(text);
            std::transform(result.begin(), result.end(), result.begin(),
                [](unsigned char character) { return static_cast<char>(std::tolower(character)); });
            return result;
        }

        template<std::size_t Count>
        bool containsAny(std::string_view name, const std::array<std::string_view, Count>& words)
        {
            return std::any_of(words.begin(), words.end(),
                [name](std::string_view word) { return name.find(word) != std::string_view::npos; });
        }

        bool isFurnishing(const assets::Scene& scene, uint32_t node)
        {
            for (uint32_t depth = 0; depth <= k_ancestorsChecked && node < scene.nodes.size(); ++depth)
            {
                const std::string name = lowered(scene.nodes[node].name);

                if (name.find(k_notFabricWord) != std::string::npos)
                {
                    return false;
                }

                if (containsAny(name, k_furnishingWords))
                {
                    return true;
                }

                node = scene.nodes[node].parent;
            }

            return false;
        }

        void applyToModels(render::Renderer& renderer, const assets::Scene& scene)
        {
            for (const auto& model : scene.skinnedModels)
            {
                for (const auto& part : model.parts)
                {
                    const std::string name = lowered(part.materialName);

                    if (name.find(k_notSkinWord) == std::string::npos && containsAny(name, k_skinWords))
                    {
                        renderer.setMaterialShading(part.material, k_skin);
                    }
                    else if (containsAny(name, k_clothingWords))
                    {
                        renderer.setMaterialShading(part.material, k_clothing);
                    }
                }
            }
        }
    } //namespace

    MaterialTimingDrill parseMaterialTimingDrill(std::span<const char* const> arguments)
    {
        MaterialTimingDrill drill{};
#if LOCKSTEP_DEV_TOOLS
        for (std::size_t index = 0; index + 1 < arguments.size(); ++index)
        {
            if (arguments[index] == nullptr || std::string_view(arguments[index]) != k_timingDrillFlag
                || arguments[index + 1] == nullptr)
            {
                continue;
            }

            const std::string_view value(arguments[index + 1]);
            uint64_t period = 0;

            if (std::from_chars(value.data(), value.data() + value.size(), period).ec != std::errc{})
            {
                std::clog << "Game: " << k_timingDrillFlag << " wants a frame count\n";
                continue;
            }

            drill.period = period;
        }
#else
        static_cast<void>(arguments);
#endif
        return drill;
    }

    void updateMaterialTimingDrill(MaterialTimingDrill& drill, render::Renderer& renderer)
    {
        if (drill.period == 0 || ++drill.frame % drill.period != 0)
        {
            return;
        }

        render::MaterialShadingSettings& materials = renderer.settings().materials;
        const bool isOn = (drill.frame / drill.period) % 2 == 0;
        materials.skin = isOn;
        materials.fabricSheen = isOn;
        materials.characterSurfaces = isOn;
        materials.decalRelief = isOn;
        std::clog << "Game: material A/B: " << (isOn ? "on" : "off") << " at frame " << drill.frame << '\n';
    }

    void applyMaterialLooks(render::Renderer& renderer, const assets::Scene& scene)
    {
        applyToModels(renderer, scene);

        for (const assets::SceneInstance& instance : scene.instances)
        {
            if (instance.mesh < scene.meshes.size() && scene.meshes[instance.mesh].alphaMode != render::AlphaMode::Blend
                && isFurnishing(scene, instance.node))
            {
                renderer.setMaterialShading(scene.meshes[instance.mesh].material, k_furnishing);
            }
        }
    }
} //namespace game
