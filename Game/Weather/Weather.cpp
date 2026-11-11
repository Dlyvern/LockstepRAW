#include "Weather/Weather.hpp"

#include "GameState.hpp"

#include <Lockstep/App/GameApi.hpp>
#include <Lockstep/Assets/Scene.hpp>
#include <Lockstep/Render/Renderer.hpp>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <iostream>
#include <string>

namespace game
{
    namespace
    {
        namespace render = lockstep::render;

        using lockstep::app::EngineContext;
        using lockstep::app::GameState;

        constexpr std::string_view k_weatherFlag{"--weather"};
        constexpr std::string_view k_rainMarker{"level_weather_rain"};
        constexpr double k_rippleWrapSeconds{3600.0};
        constexpr float k_litterMinWind{3.5f};
        constexpr float k_drillStrikeSeconds{4.0f};
        constexpr float k_drillFirstStrike{2.0f};
        constexpr float k_litterFullWind{11.0f};

        std::string lowered(std::string_view text)
        {
            std::string result(text);
            std::transform(result.begin(), result.end(), result.begin(),
                [](unsigned char character) { return static_cast<char>(std::tolower(character)); });
            return result;
        }

        float approach(float value, float target, float seconds, float duration)
        {
            const float step = duration > 0.0f ? seconds / duration : 1.0f;
            return value + std::clamp(target - value, -step, step);
        }

        void pickWeather(Weather& weather, const lockstep::assets::Scene* scene)
        {
            weather.scene = scene;
            weather.kind = weather.forced.value_or(scene != nullptr ? levelWeather(*scene) : WeatherKind::Clear);
            weather.wetness = weather.kind == WeatherKind::Rain ? 1.0f : 0.0f;
            weather.lookAmount = weather.kind == WeatherKind::Rain ? 1.0f : 0.0f;
            weather.appliedLook = -1.0f;
            weather.lookBase.reset();

            if (scene != nullptr)
            {
                std::clog << "Game: weather '" << weatherName(weather.kind) << "'"
                          << (weather.forced ? " (--weather)" : "") << '\n';
            }
        }

        void presentLook(Weather& weather, render::RenderSettings& settings, float frameSeconds)
        {
            const float target = weather.tuning.overcastLook ? rainAmount(weather) : 0.0f;
            weather.lookAmount = approach(weather.lookAmount, target, frameSeconds, weather.tuning.lookSeconds);

            if (weather.appliedLook == weather.lookAmount)
            {
                return;
            }

            if (!weather.lookBase)
            {
                if (weather.lookAmount <= 0.0f && weather.appliedLook < 0.0f)
                {
                    weather.appliedLook = 0.0f;
                    return;
                }

                weather.lookBase = captureRainLookBase(settings);
            }

            applyRainLook(settings, *weather.lookBase, weather.lookAmount);
            weather.appliedLook = weather.lookAmount;
        }

        void presentWetness(Weather& weather, render::WetnessSettings& wetness, float worldSeconds)
        {
            const float rain = rainAmount(weather);
            const float target = rain > 0.0f ? std::max(rain, 0.6f) : 0.0f;
            const float duration = target > weather.wetness ? weather.tuning.soakSeconds : weather.tuning.drySeconds;
            weather.wetness = approach(weather.wetness, target, worldSeconds, duration);
            weather.rippleSeconds = std::fmod(weather.rippleSeconds + static_cast<double>(worldSeconds),
                k_rippleWrapSeconds);

            wetness.enabled = weather.tuning.wetness && weather.wetness > 0.0f;
            wetness.amount = weather.wetness;
            wetness.ripples = rain;
            wetness.rippleTime = static_cast<float>(weather.rippleSeconds);
        }
    } //namespace

    lockstep::Result<std::optional<WeatherKind>> parseWeatherFlag(std::span<const char* const> arguments)
    {
        for (std::size_t index = 0; index < arguments.size(); ++index)
        {
            if (arguments[index] == nullptr || std::string_view(arguments[index]) != k_weatherFlag)
            {
                continue;
            }

            if (index + 1 >= arguments.size() || arguments[index + 1] == nullptr)
            {
                return lockstep::Error{lockstep::ErrorCode::InvalidArgument, "--weather needs 'rain' or 'clear'"};
            }

            const std::string value = lowered(arguments[index + 1]);

            if (value == weatherName(WeatherKind::Rain))
            {
                return std::optional<WeatherKind>(WeatherKind::Rain);
            }

            if (value == weatherName(WeatherKind::Clear))
            {
                return std::optional<WeatherKind>(WeatherKind::Clear);
            }

            return lockstep::Error{lockstep::ErrorCode::InvalidArgument,
                "--weather: '" + std::string(arguments[index + 1]) + "' is not 'rain' or 'clear'"};
        }

        return std::optional<WeatherKind>{};
    }

    WeatherKind levelWeather(const lockstep::assets::Scene& scene)
    {
        const bool hasRain = std::any_of(scene.nodes.begin(), scene.nodes.end(),
            [](const lockstep::assets::SceneNode& node) { return lowered(node.name).starts_with(k_rainMarker); });

        return hasRain ? WeatherKind::Rain : WeatherKind::Clear;
    }

    float rainAmount(const Weather& weather)
    {
        return weather.kind == WeatherKind::Rain ? std::clamp(weather.tuning.intensity, 0.0f, 1.0f) : 0.0f;
    }

    float rainSoundAmount(const Weather& weather)
    {
        return weather.tuning.sound ? rainAmount(weather) : 0.0f;
    }

    void presentWeather(GameState& state, EngineContext& engine, const WeatherView& view, float worldSeconds,
        float frameSeconds)
    {
        Weather& weather = state.weather;
        const lockstep::assets::Scene* scene = state.level ? &state.level->scene : nullptr;

        if (scene != weather.scene)
        {
            pickWeather(weather, scene);
            weather.lens.drops.clear();
        }

        render::RenderSettings& settings = engine.renderer.settings();
        presentLook(weather, settings, frameSeconds);
        presentWetness(weather, settings.wetness, worldSeconds);

        const float rain = rainAmount(weather);
        updateWind(weather.wind, weather.tuning.wind, rain, worldSeconds);
        applyWind(weather.wind, settings);
        updateLightning(weather.lightning, weather.tuning.lightning, rain, settings.clouds.baseHeight, worldSeconds);
        applyLightning(weather.lightning, weather.tuning.lightning, settings);

        const lockstep::math::Vec2 screen = engine.renderer.screenSize();
        updateLensDrops(weather.lens, weather.tuning.lens, LensDropFrame{.rain = rain,
            .eye = view.eye,
            .forward = view.forward,
            .wind = windVelocity(weather.wind),
            .physics = &engine.physics,
            .interiors = state.combatAudio.interiors,
            .seconds = view.playerSeconds,
            .aspectRatio = screen.y > 0.0f ? screen.x / screen.y : 16.0f / 9.0f}, engine.renderer);
    }

    float litterRate(const Weather& weather)
    {
        if (!weather.tuning.litter)
        {
            return 0.0f;
        }

        const float gusting = std::clamp((weather.wind.speed - k_litterMinWind) / (k_litterFullWind - k_litterMinWind),
            0.0f, 1.0f);
        return gusting * gusting * std::max(weather.tuning.litterDensity, 0.0f);
    }

    void startLightningDrill(Weather& weather)
    {
        LightningTuning& lightning = weather.tuning.lightning;
        lightning.minRain = 0.0f;
        lightning.minSeconds = k_drillStrikeSeconds;
        lightning.maxSeconds = k_drillStrikeSeconds;
        weather.lightning.untilStrike = k_drillFirstStrike;
    }

    void resetWeatherLook(Weather& weather)
    {
        weather.lookBase.reset();
        weather.appliedLook = -1.0f;
    }

    std::string_view weatherName(WeatherKind weather)
    {
        switch (weather)
        {
            case WeatherKind::Clear:
            {
                return "clear";
            }
            case WeatherKind::Rain:
            {
                return "rain";
            }
        }

        return "clear";
    }
} //namespace game
