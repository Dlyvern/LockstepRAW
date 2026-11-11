#ifndef LOCKSTEP_GAME_PRESENTATION_LEVEL_AMBIENCE_HPP
#define LOCKSTEP_GAME_PRESENTATION_LEVEL_AMBIENCE_HPP

#include <Lockstep/Assets/Scene.hpp>

namespace lockstep::render
{
    class Renderer;
} //namespace lockstep::render

namespace game
{
    // Hands the level's interior ambient zones to the renderer, replacing any earlier ones. A zone
    // is a node named "Zone_Interior_<anything>": a Blender cube empty (display size 1) whose
    // scale makes it cover the interior, its walls and its roof; the zone is the world box around
    // the transformed [-1, 1] cube. Also hands over the level's baked irradiance volume (bounce
    // light), or removes the previous one when the level has none. Presentation only.
    // renderer: the renderer.
    // scene:    the loaded level.
    // Returns:  the number of zones found.
    uint32_t applyLevelAmbientZones(lockstep::render::Renderer& renderer, const lockstep::assets::Scene& scene);
} //namespace game

#endif //LOCKSTEP_GAME_PRESENTATION_LEVEL_AMBIENCE_HPP
