#ifndef LOCKSTEP_GAME_CHARACTERS_CHARACTER_MODEL_HPP
#define LOCKSTEP_GAME_CHARACTERS_CHARACTER_MODEL_HPP

#include "Characters/GearLooks.hpp"

#include <Lockstep/Anim/SkinnedModel.hpp>
#include <Lockstep/Assets/Scene.hpp>

#include <cstdint>
#include <optional>
#include <string_view>

namespace lockstep::assets
{
    class FileSystem;
} //namespace lockstep::assets

namespace lockstep::render
{
    class Renderer;
} //namespace lockstep::render

namespace game
{
    // One skinned character model loaded from a cooked scene.
    // scene:     the loaded scene (owns the model's GPU handles).
    // model:     index of the model in scene.skinnedModels.
    // gearMasks: the parts drawn for each head gear look.
    struct CharacterModel
    {
        lockstep::assets::Scene scene;
        uint32_t model{0};
        GearMasks gearMasks{};

        // Returns: the skinned model.
        [[nodiscard]] const lockstep::anim::SkinnedModel& skinned() const
        {
            return scene.skinnedModels[model];
        }
    };

    // Loads a skinned model from a cooked scene of the mounted content.
    // renderer: uploads the model.
    // files:    the mounted content.
    // scene:    virtual path of the cooked scene.
    // model:    name of the skinned model in it; empty takes the first.
    // Returns:  the model, or nothing when the scene isn't mounted, fails to load or has no such
    //           model (the reason is logged).
    [[nodiscard]] std::optional<CharacterModel> loadCharacterModel(lockstep::render::Renderer& renderer,
        const lockstep::assets::FileSystem& files, std::string_view scene, std::string_view model);
} //namespace game

#endif //LOCKSTEP_GAME_CHARACTERS_CHARACTER_MODEL_HPP
