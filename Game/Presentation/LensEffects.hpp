#ifndef LOCKSTEP_GAME_PRESENTATION_LENS_EFFECTS_HPP
#define LOCKSTEP_GAME_PRESENTATION_LENS_EFFECTS_HPP

namespace lockstep::render
{
    struct RenderSettings;
} //namespace lockstep::render

namespace game
{
    // Drives the camera lens from game state, every presented frame: depth of field fades in while
    // bullet-time slows the World domain (focus on what the player looks at, the rest of the room
    // softens) and fades out after. Writes only RenderSettings::depthOfField.strength; the level
    // look and the dev tools own every other lens setting. Presentation only.
    // settings:   the renderer settings.
    // worldScale: the World time domain's scale this frame (below 1 is bullet-time).
    // seconds:    real frame time in seconds.
    void presentLensEffects(lockstep::render::RenderSettings& settings, float worldScale, float seconds);
} //namespace game

#endif //LOCKSTEP_GAME_PRESENTATION_LENS_EFFECTS_HPP
