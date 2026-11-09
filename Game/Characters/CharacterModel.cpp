#include "Characters/CharacterModel.hpp"

#include <Lockstep/Assets/FileSystem.hpp>

#include <iostream>
#include <utility>

namespace game
{
    namespace
    {
        namespace assets = lockstep::assets;

        std::optional<uint32_t> findModel(const assets::Scene& scene, std::string_view name)
        {
            for (uint32_t model = 0; model < scene.skinnedModels.size(); ++model)
            {
                if (name.empty() || scene.skinnedModels[model].name == name)
                {
                    return model;
                }
            }

            return std::nullopt;
        }
    } //namespace

    std::optional<CharacterModel> loadCharacterModel(lockstep::render::Renderer& renderer,
        const assets::FileSystem& files, std::string_view scene, std::string_view model)
    {
        if (!files.exists(scene))
        {
            std::clog << "Game: character '" << scene << "' not mounted (add --content Content/cooked/characters.pak)\n";
            return std::nullopt;
        }

        auto loaded = assets::loadCookedScene(renderer, files, scene);

        if (!loaded)
        {
            std::clog << "Game: character '" << scene << "' not loaded: " << loaded.error().message << '\n';
            return std::nullopt;
        }

        const std::optional<uint32_t> index = findModel(loaded.value(), model);

        if (!index)
        {
            std::clog << "Game: '" << scene << "' has no skinned model '" << model << "'\n";
            return std::nullopt;
        }

        return CharacterModel{.scene = std::move(loaded).value(), .model = *index};
    }
} //namespace game
