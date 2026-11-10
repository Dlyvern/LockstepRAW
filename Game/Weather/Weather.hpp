#ifndef LOCKSTEP_GAME_WEATHER_WEATHER_HPP
#define LOCKSTEP_GAME_WEATHER_WEATHER_HPP

#include "Weather/RainLook.hpp"

#include <Lockstep/Core/Result.hpp>

#include <cstdint>
#include <optional>
#include <span>
#include <string_view>

namespace lockstep::app
{
    struct EngineContext;
    struct GameState;
} //namespace lockstep::app

namespace lockstep::assets
{
    struct Scene;
} //namespace lockstep::assets

namespace game
{
    // The sky's weather.
    // Clear: no rain.
    // Rain:  rain streaks and splashes, wet streets with puddles, rain sound and distant thunder,
    //        and a darker, overcast, hazier variant of the level's look.
    enum class WeatherKind : uint8_t
    {
        Clear = 0,
        Rain
    };

    // Switches and rates of the weather, editable from the dev panel.
    // intensity:      strength of the rain, 0 to 1 (streak density, splashes, ripples, sound).
    // particles:      rain streaks around the camera (outdoors; they die on reaching a roof).
    // splashes:       droplet splashes on the ground near the camera.
    // wetness:        wet surfaces and puddles (RenderSettings::wetness).
    // overcastLook:   darken and haze the level's look while it rains.
    // sound:          the rain loops and thunder (volumes in the combat audio tuning).
    // streakDensity:  factor on the rain streaks' spawn rate.
    // splashRate:     splashes per second at intensity 1.
    // soakSeconds:    World seconds dry surfaces take to soak once rain starts.
    // drySeconds:     World seconds wet surfaces take to dry once it stops.
    // lookSeconds:    seconds the overcast look takes to come and go.
    struct WeatherTuning
    {
        float intensity{1.0f};
        bool particles{true};
        bool splashes{true};
        bool wetness{true};
        bool overcastLook{true};
        bool sound{true};
        float streakDensity{1.0f};
        float splashRate{160.0f};
        float soakSeconds{30.0f};
        float drySeconds{150.0f};
        float lookSeconds{4.0f};
    };

    // The weather on this machine. Presentation only, never replicated: each machine picks it from
    // the level's marker (or --weather) and animates it with its own World time, so both players
    // see rain where the other does, but not the same drops.
    // kind:          the current weather.
    // forced:        the --weather override; it beats the level's marker.
    // tuning:        switches and rates.
    // scene:         the level the weather was picked for.
    // wetness:       how wet surfaces are, 0 to 1, following the rain with a delay.
    // lookAmount:    how far the overcast look is blended in, 0 to 1.
    // appliedLook:   lookAmount last written to the render settings (negative: not yet).
    // lookBase:      the level look's values the overcast look blends from, captured the first time
    //                the overcast look changes after a level or look preset was applied.
    // rippleSeconds: World seconds of the puddle ripple animation.
    struct Weather
    {
        WeatherKind kind{WeatherKind::Clear};
        std::optional<WeatherKind> forced{};
        WeatherTuning tuning{};
        const lockstep::assets::Scene* scene{nullptr};
        float wetness{0.0f};
        float lookAmount{0.0f};
        float appliedLook{-1.0f};
        std::optional<RainLookBase> lookBase{};
        double rippleSeconds{0.0};
    };

    // Reads `--weather rain|clear` from the command line.
    // arguments: the command line, without the program name.
    // Returns:   the forced weather, nothing without the flag, or an error for a missing or unknown
    //            value.
    [[nodiscard]] lockstep::Result<std::optional<WeatherKind>> parseWeatherFlag(std::span<const char* const> arguments);

    // Picks a level's weather: rain if it has a node named "Level_Weather_Rain" (case-insensitive,
    // Blender's ".001" suffixes are fine), else clear.
    // scene:   the loaded level.
    // Returns: its weather.
    [[nodiscard]] WeatherKind levelWeather(const lockstep::assets::Scene& scene);

    // weather: the weather state.
    // Returns: how hard it rains now, 0 (clear) to 1.
    [[nodiscard]] float rainAmount(const Weather& weather);

    // weather: the weather state.
    // Returns: the rain amount the rain sounds should play at (0 with sound off).
    [[nodiscard]] float rainSoundAmount(const Weather& weather);

    // Updates the weather for this frame: picks it again when the level changed, eases wetness and
    // the overcast look, and writes the wetness and look into the render settings. Call before the
    // combat audio and the effects run.
    // state:        the game state (weather, level).
    // engine:       the engine (render settings).
    // worldSeconds: World time since the previous frame (bullet-time slows the soaking and ripples).
    // frameSeconds: real time since the previous frame (the look fades in real time).
    void presentWeather(lockstep::app::GameState& state, lockstep::app::EngineContext& engine, float worldSeconds,
        float frameSeconds);

    // Forgets the captured level look, so the next overcast blend starts from the settings as they
    // are now. Call after applying a look preset.
    // weather: the weather state.
    void resetWeatherLook(Weather& weather);

    // weather: a weather kind.
    // Returns: its name ("clear", "rain").
    [[nodiscard]] std::string_view weatherName(WeatherKind weather);

#if LOCKSTEP_DEV_TOOLS
    // Draws the weather section of the Game dev panel: weather, intensity and switches.
    // weather: the weather state.
    void weatherDevSection(Weather& weather);
#endif
} //namespace game

#endif //LOCKSTEP_GAME_WEATHER_WEATHER_HPP
