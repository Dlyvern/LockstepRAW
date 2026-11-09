#ifndef LOCKSTEP_GAME_PRESENTATION_MIRRORS_HPP
#define LOCKSTEP_GAME_PRESENTATION_MIRRORS_HPP

#include <Lockstep/Render/Views.hpp>

namespace lockstep::assets
{
    struct Scene;
} //namespace lockstep::assets

namespace lockstep::render
{
    class Renderer;
} //namespace lockstep::render

namespace game
{
    // Render layer of what only mirrors show: the local player's own pawn, which the first-person
    // camera never draws. Later epilogues can put something else on it.
    inline constexpr lockstep::render::RenderLayers k_mirrorOnlyLayer{1u << 1};

    // Registers the level's mirrors (the mesh instances under "Interact_Mirror" nodes) as planar
    // reflectors for this frame. The caller draws the local pawn on k_mirrorOnlyLayer so the
    // player sees themselves in them. Presentation only; never replicated.
    // renderer: the renderer.
    // scene:    the loaded level, or null without one (then nothing is registered).
    void presentMirrors(lockstep::render::Renderer& renderer, const lockstep::assets::Scene* scene);
} //namespace game

#endif //LOCKSTEP_GAME_PRESENTATION_MIRRORS_HPP
