#ifndef LOCKSTEP_GAME_PRESENTATION_VISUALS_HPP
#define LOCKSTEP_GAME_PRESENTATION_VISUALS_HPP

#include "Archetypes.hpp"
#include "Level/Arena.hpp"
#include "Player/PlayerStatus.hpp"

#include <Lockstep/Core/Math.hpp>
#include <Lockstep/Core/Result.hpp>
#include <Lockstep/Core/Transform.hpp>
#include <Lockstep/Render/Camera.hpp>
#include <Lockstep/Render/Resources.hpp>
#include <Lockstep/World/Entities.hpp>

#include <cstdint>

namespace lockstep::render
{
    class Renderer;
} //namespace lockstep::render

namespace game
{
    namespace math = lockstep::math;

    // The meshes and materials the game draws with, owned by the Renderer.
    // cube:         unit cube, stretched into the arena, pawns and enemy.
    // floor:        checkerboard.
    // solid:        walls, cover, steps and ramps.
    // player1:      the host player's colour.
    // player2:      the client player's colour.
    // enemy:        the enemy's colour.
    // crosshair:    the aim dot.
    // healthFull:  a health pip that is still filled.
    // healthEmpty: a health pip that was lost.
    // prompt:      the marks around the aim dot while something usable is in reach.
    // ghost:       Echo ghost pawns.
    struct Visuals
    {
        lockstep::render::MeshHandle cube{};
        lockstep::render::MaterialHandle floor{};
        lockstep::render::MaterialHandle solid{};
        lockstep::render::MaterialHandle player1{};
        lockstep::render::MaterialHandle player2{};
        lockstep::render::MaterialHandle enemy{};
        lockstep::render::MaterialHandle crosshair{};
        lockstep::render::MaterialHandle healthFull{};
        lockstep::render::MaterialHandle healthEmpty{};
        lockstep::render::MaterialHandle prompt{};
        lockstep::render::MaterialHandle ghost{};
    };

    // Creates the game's meshes and materials.
    // renderer: the renderer that owns them.
    // Returns:  the handles, or an error if an upload failed.
    [[nodiscard]] lockstep::Result<Visuals> createVisuals(lockstep::render::Renderer& renderer);

    // Queues the arena's boxes for drawing.
    // renderer: the renderer.
    // visuals:  the game's meshes and materials.
    // arena:    the arena.
    void drawArena(lockstep::render::Renderer& renderer, const Visuals& visuals, const Arena& arena);

    // Queues an entity for drawing with the look of its archetype. Unknown archetypes are skipped.
    // renderer:  the renderer.
    // visuals:   the game's meshes and materials.
    // archetype: what the entity is.
    // owner:     who controls it; picks the player colour of a pawn.
    // transform: where to draw it (already interpolated).
    void drawEntity(lockstep::render::Renderer& renderer, const Visuals& visuals, Archetype archetype,
        lockstep::world::EntityOwner owner, const math::Transform& transform);

    // Queues the aim dot a short way in front of the eyes.
    // renderer:  the renderer.
    // visuals:   the game's meshes and materials.
    // eye:       the camera position.
    // direction: unit look direction.
    void drawCrosshair(lockstep::render::Renderer& renderer, const Visuals& visuals, const math::Vec3& eye,
        const math::Vec3& direction);

    // Queues four marks around the aim dot: something usable is in reach (press E).
    // renderer:  the renderer.
    // visuals:   the game's meshes and materials.
    // eye:       the camera position.
    // direction: unit look direction.
    void drawInteractPrompt(lockstep::render::Renderer& renderer, const Visuals& visuals, const math::Vec3& eye,
        const math::Vec3& direction);

    // Queues the local player's health as a row of pips at the bottom of the view.
    // renderer:  the renderer.
    // visuals:   the game's meshes and materials.
    // eye:       the camera position.
    // direction: unit look direction.
    // status:    the local player's status; a dead player shows only empty pips.
    // maxHealth: health of a full row.
    void drawHealth(lockstep::render::Renderer& renderer, const Visuals& visuals, const math::Vec3& eye,
        const math::Vec3& direction, const PlayerStatus& status, int32_t maxHealth);

    // Builds a first-person camera.
    // eye:       the camera position.
    // direction: unit look direction.
    // zoom:      0 for the normal field of view, 1 for the aim-down-sights zoom (blends between).
    // up:        the camera's up vector (tilted for camera roll).
    // fovOffset: radians added to the field of view before the zoom (running, bullet-time).
    // Returns:   the camera.
    [[nodiscard]] lockstep::render::Camera firstPersonCamera(const math::Vec3& eye, const math::Vec3& direction,
        float zoom, const math::Vec3& up = math::Vec3{0.0f, 1.0f, 0.0f}, float fovOffset = 0.0f);
} //namespace game

#endif //LOCKSTEP_GAME_PRESENTATION_VISUALS_HPP
