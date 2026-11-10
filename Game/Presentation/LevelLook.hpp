#ifndef LOCKSTEP_GAME_PRESENTATION_LEVEL_LOOK_HPP
#define LOCKSTEP_GAME_PRESENTATION_LEVEL_LOOK_HPP

#include <Lockstep/Assets/Scene.hpp>

#include <cstdint>
#include <string_view>

namespace lockstep::render
{
    class Renderer;
    struct RenderSettings;
} //namespace lockstep::render

namespace game
{
    // A level's time of day and grade. Each preset sets the sun, sky, image-based light, exposure
    // range, interior zones, grading, vignette and grain as one consistent look.
    // Day:     clear afternoon sun (the station, the district).
    // Evening: sun a few degrees above the horizon, warm and weak; dim blue sky; local lights
    //          (street lamps, light bars) do most of the lighting.
    // Sunset:  golden hour: sun 6 degrees up in the west-southwest, strong orange light that
    //          rakes south facades; warm glowing horizon under a deep blue zenith; warm haze over
    //          distant buildings; bright cool sky fill so shadows stay readable; warm/teal grade.
    // Night:   no sun, faint sky; only local lights.
    enum class LookPreset : uint8_t
    {
        Day = 0,
        Evening,
        Sunset,
        Night
    };

    // Applies a preset to the render settings, replacing everything it covers.
    // settings: the renderer settings.
    // preset:   the look.
    void applyLookPreset(lockstep::render::RenderSettings& settings, LookPreset preset);

    // Picks the level's look from a node named "Level_Look_<Preset>" ("Level_Look_Evening",
    // "Level_Look_Sunset", "Level_Look_Night", "Level_Look_Day"; case-insensitive, Blender's ".001" suffixes are fine)
    // and applies it; a level without one gets Day. Presentation only.
    // renderer: the renderer.
    // scene:    the loaded level.
    // Returns:  the preset applied.
    LookPreset applyLevelLook(lockstep::render::Renderer& renderer, const lockstep::assets::Scene& scene);

    // preset:  a look.
    // Returns: its name ("day", "evening", "sunset", "night").
    [[nodiscard]] std::string_view lookPresetName(LookPreset preset);
} //namespace game

#endif //LOCKSTEP_GAME_PRESENTATION_LEVEL_LOOK_HPP
