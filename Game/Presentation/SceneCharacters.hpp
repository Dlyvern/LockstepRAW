#ifndef LOCKSTEP_GAME_PRESENTATION_SCENE_CHARACTERS_HPP
#define LOCKSTEP_GAME_PRESENTATION_SCENE_CHARACTERS_HPP

#include <Lockstep/Anim/SkinnedModel.hpp>
#include <Lockstep/Assets/Scene.hpp>

#include <Lockstep/Core/Math.hpp>

#include <span>
#include <vector>

namespace lockstep::render
{
    class Renderer;
} //namespace lockstep::render

namespace game
{
    // How a level's skinned models are animated, editable from the dev panel. Presentation only.
    // isBlending:  play a 1D blend over all clips (clip n at parameter n) instead of one clip.
    // clip:        the clip played while not blending.
    // parameter:   the blend parameter while blending.
    // fadeSeconds: crossfade length when the motion changes.
    // speed:       playback rate.
    // isViewModel: dev test of first-person viewmodel rendering: the first character is drawn
    //              on render::k_viewModelLayer with its eye point at the camera, facing the view
    //              direction, instead of at its level placement.
    // viewModelOffset: camera-space shift of the viewmodel in metres (x right, y up, z forward);
    //              the test model holds its weapon at chest height, below a 55 degree view.
    // viewModelYaw: turn of the viewmodel about its eye point, in degrees; the test model's hold
    //              pose points the weapon across the body (towards its +X).
    struct CharacterControls
    {
        bool isBlending{false};
        int clip{0};
        float parameter{0.0f};
        float fadeSeconds{0.3f};
        float speed{1.0f};
        bool isViewModel{false};
        float viewModelYaw{50.0f};
        lockstep::math::Vec3 viewModelOffset{0.05f, 0.3f, 0.15f};
    };

    // The animated skinned models placed in a loaded level (Scene::skinnedInstances), one
    // anim::SkinnedInstance each, all following the same controls.
    struct SceneCharacters
    {
        std::vector<lockstep::anim::SkinnedInstance> instances;
        CharacterControls controls{};
    };

    // Creates an animated instance for every skinned instance of a scene, playing its first clip.
    // scene:     the loaded level.
    // arguments: command-line arguments; "--viewmodel-test" starts with
    //            CharacterControls::isViewModel on.
    // Returns:   the characters.
    [[nodiscard]] SceneCharacters createSceneCharacters(const lockstep::assets::Scene& scene,
        std::span<const char* const> arguments);

    // Applies the controls, advances every character and submits it for the next render().
    // renderer:     the renderer the scene was loaded with.
    // scene:        the loaded level.
    // characters:   the characters from createSceneCharacters().
    // frameSeconds: presentation time since the last frame.
    // eye:          the camera position, where a viewmodel character's eye point goes.
    // direction:    the camera's unit view direction, which a viewmodel character faces.
    void presentSceneCharacters(lockstep::render::Renderer& renderer, const lockstep::assets::Scene& scene,
        SceneCharacters& characters, float frameSeconds, const lockstep::math::Vec3& eye,
        const lockstep::math::Vec3& direction);

#if LOCKSTEP_DEV_TOOLS
    // Draws the characters section of the Game dev panel (clip, blend, crossfade, speed). Shows
    // nothing when the level has no skinned models.
    // scene:      the loaded level.
    // characters: the characters.
    void sceneCharactersDevSection(const lockstep::assets::Scene& scene, SceneCharacters& characters);
#endif
} //namespace game

#endif //LOCKSTEP_GAME_PRESENTATION_SCENE_CHARACTERS_HPP
