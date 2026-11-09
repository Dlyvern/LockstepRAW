#ifndef LOCKSTEP_GAME_PRESENTATION_MIRRORS_HPP
#define LOCKSTEP_GAME_PRESENTATION_MIRRORS_HPP

#include "Presentation/Visuals.hpp"

#include <Lockstep/Core/Transform.hpp>
#include <Lockstep/Render/Views.hpp>
#include <Lockstep/World/Entities.hpp>

#include <optional>

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
    // reflectors for this frame, and draws the local pawn on k_mirrorOnlyLayer so the player sees
    // themselves in them. Presentation only; never replicated.
    // renderer:   the renderer.
    // scene:      the loaded level, or null without one (then only the pawn is skipped too).
    // visuals:    the game's meshes and materials.
    // localPawn:  the local pawn's interpolated transform, if the player has one.
    // localOwner: which player is local (picks the pawn colour).
    void presentMirrors(lockstep::render::Renderer& renderer, const lockstep::assets::Scene* scene,
        const Visuals& visuals, const std::optional<lockstep::math::Transform>& localPawn,
        lockstep::world::EntityOwner localOwner);
} //namespace game

#endif //LOCKSTEP_GAME_PRESENTATION_MIRRORS_HPP
