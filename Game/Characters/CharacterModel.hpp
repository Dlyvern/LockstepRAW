#ifndef LOCKSTEP_GAME_CHARACTERS_CHARACTER_MODEL_HPP
#define LOCKSTEP_GAME_CHARACTERS_CHARACTER_MODEL_HPP

#include "Characters/GearLooks.hpp"
#include "Characters/MotionSets.hpp"

#include <Lockstep/Anim/LocomotionPose.hpp>
#include <Lockstep/Anim/RootMotion.hpp>
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
    // Joint name endings of the Mixamo skeleton every character uses, for the locomotion rig.
    inline constexpr lockstep::anim::LocomotionRigNames k_mixamoLocomotionRig{.hips = "Hips",
        .spine = {"Spine", "Spine1", "Spine2", ""},
        .upperLegs = {"LeftUpLeg", "RightUpLeg"},
        .lowerLegs = {"LeftLeg", "RightLeg"},
        .feet = {"LeftFoot", "RightFoot"},
        .toes = {"LeftToeBase", "RightToeBase"}};

    // One skinned character model loaded from a cooked scene.
    // scene:         the loaded scene (owns the model's GPU handles).
    // model:         index of the model in scene.skinnedModels.
    // gearMasks:     the parts drawn for each head gear look.
    // locomotionRig: the joints foot IK and the locomotion warps move, or nothing for a model
    //                without the Mixamo legs (first-person arms).
    // rootJoint:     the hips, where the turn of turning clips is taken out.
    // motionSets:    the motion-matching sets built from its MM_ clips, or nothing without them.
    struct CharacterModel
    {
        lockstep::assets::Scene scene;
        uint32_t model{0};
        GearMasks gearMasks{};
        std::optional<lockstep::anim::LocomotionRig> locomotionRig{};
        lockstep::anim::RootJoint rootJoint{};
        std::optional<MotionSets> motionSets{};

        // Returns: the skinned model.
        [[nodiscard]] const lockstep::anim::SkinnedModel& skinned() const
        {
            return scene.skinnedModels[model];
        }
    };

    // Loads a skinned model from a cooked scene of the mounted content, with its locomotion rig and
    // motion sets.
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
