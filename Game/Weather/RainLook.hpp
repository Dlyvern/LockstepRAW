#ifndef LOCKSTEP_GAME_WEATHER_RAIN_LOOK_HPP
#define LOCKSTEP_GAME_WEATHER_RAIN_LOOK_HPP

#include <Lockstep/Render/RenderSettings.hpp>

namespace game
{
    // The parts of a level's look the overcast rain look changes, as they were before it.
    struct RainLookBase
    {
        lockstep::render::SkySettings sky{};
        lockstep::render::ImageBasedLightingSettings ibl{};
        lockstep::render::FogSettings fog{};
        lockstep::render::VolumetricFogSettings volumetricFog{};
        lockstep::render::ColorGradingSettings colorGrading{};
        lockstep::render::CloudSettings clouds{};
    };

    // settings: the render settings with the level's look applied.
    // Returns:  the values the rain look blends from.
    [[nodiscard]] RainLookBase captureRainLookBase(const lockstep::render::RenderSettings& settings);

    // Writes a blend of the level's look towards its rainy variant into the render settings: a
    // weaker, greyer sun behind a hazier sky, a dimmer sunset glow, thicker distance fog and haze
    // (cool grey), a low, dark, nearly closed cloud deck, and slightly less saturation. The clouds'
    // drift offset is left alone (the wind owns it).
    // settings: the render settings to write.
    // base:     the level's look before rain.
    // amount:   0 the level's look as it was, 1 fully overcast.
    void applyRainLook(lockstep::render::RenderSettings& settings, const RainLookBase& base, float amount);
} //namespace game

#endif //LOCKSTEP_GAME_WEATHER_RAIN_LOOK_HPP
