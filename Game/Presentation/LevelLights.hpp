#ifndef LOCKSTEP_GAME_PRESENTATION_LEVEL_LIGHTS_HPP
#define LOCKSTEP_GAME_PRESENTATION_LEVEL_LIGHTS_HPP

#include <Lockstep/Assets/Scene.hpp>
#include <Lockstep/Core/Math.hpp>

namespace lockstep::render
{
    class Renderer;
} //namespace lockstep::render

namespace game
{
    // Submits the level's authored point and spot lights that matter most to a viewer at `eye`:
    // those whose range reaches within a few metres of it, nearest edge first, at most the
    // renderer's k_maxLocalLights. Lights near the cut (the reach limit, or the first light that
    // did not fit) fade out over a few metres, so lights swapping in and out do not pop.
    // Directional lights are ignored (the sun comes from the sky). A light whose name contains
    // "_Shadow" (e.g. "L_G_Lobby1_Shadow") casts shadows (the renderer's budget picks the nearest).
    // Presentation only.
    // renderer: the renderer.
    // scene:    the current level.
    // eye:      the camera position.
    void submitLevelLights(lockstep::render::Renderer& renderer, const lockstep::assets::Scene& scene,
        const lockstep::math::Vec3& eye);
} //namespace game

#endif //LOCKSTEP_GAME_PRESENTATION_LEVEL_LIGHTS_HPP
