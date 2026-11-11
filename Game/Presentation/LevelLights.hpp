#ifndef LOCKSTEP_GAME_PRESENTATION_LEVEL_LIGHTS_HPP
#define LOCKSTEP_GAME_PRESENTATION_LEVEL_LIGHTS_HPP

#include <Lockstep/Assets/Scene.hpp>
#include <Lockstep/Core/Math.hpp>
#include <Lockstep/Render/Lighting.hpp>

#include <cstddef>
#include <optional>
#include <span>

namespace lockstep::render
{
    class Renderer;
} //namespace lockstep::render

namespace game
{
    // A per-frame change to one authored light (animation).
    // intensity: multiplier on the authored intensity.
    // color:     replaces the authored colour when set.
    struct LightOverride
    {
        float intensity{1.0f};
        std::optional<lockstep::math::Vec3> color{};
    };

    // Submits the level's authored point and spot lights that matter most to a viewer at `eye`:
    // those whose range reaches within a few metres of it, nearest edge first, at most the
    // renderer's k_maxLocalLights. Lights near the cut (the reach limit, or the first light that
    // did not fit) fade out over a few metres, so lights swapping in and out do not pop.
    // Directional lights are ignored (the sun comes from the sky). A light whose name contains
    // "_Shadow" (e.g. "L_G_Lobby1_Shadow") casts shadows (the renderer's budget picks the nearest).
    // A point light named "_Tube" is a fluorescent tube along the light object's local X axis, as
    // long in metres as the object's X scale (Fluorescent profile unless another is named).
    // "_Downlight", "_WallWash", "_Grille", "_Fluorescent" pick an emission profile around the
    // light's local -Z (Blender lights point down -Z), its +X being the profile's side.
    // Presentation only.
    // renderer: the renderer.
    // scene:    the current level.
    // eye:      the camera position.
    // overrides: per light, indexed like scene.lights; empty (or shorter) leaves the rest as authored.
    //           A light overridden to 0 is skipped, unless it casts shadows (then it stays
    //           submitted dark, so the shadow budget does not move to other lights while it flickers).
    // maxLights: most lights submitted (fewer than k_maxLocalLights when other lights, such as
    //           screens, reserve slots).
    void submitLevelLights(lockstep::render::Renderer& renderer, const lockstep::assets::Scene& scene,
        const lockstep::math::Vec3& eye, std::span<const LightOverride> overrides = {},
        std::size_t maxLights = lockstep::render::k_maxLocalLights);
} //namespace game

#endif //LOCKSTEP_GAME_PRESENTATION_LEVEL_LIGHTS_HPP
