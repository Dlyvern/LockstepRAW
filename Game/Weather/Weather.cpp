#include "Weather/Weather.hpp"

#include "GameState.hpp"

#include <Lockstep/App/GameApi.hpp>
#include <Lockstep/Assets/Scene.hpp>
#include <Lockstep/Render/Renderer.hpp>

#if LOCKSTEP_DEV_TOOLS
#include <imgui.h>
#endif

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

    void presentWeather(GameState& state, EngineContext& engine, float worldSeconds, float frameSeconds)
    {
        Weather& weather = state.weather;
        const lockstep::assets::Scene* scene = state.level ? &state.level->scene : nullptr;

        if (scene != weather.scene)
        {
            pickWeather(weather, scene);
        }

        render::RenderSettings& settings = engine.renderer.settings();
        presentLook(weather, settings, frameSeconds);
        presentWetness(weather, settings.wetness, worldSeconds);
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

#if LOCKSTEP_DEV_TOOLS
    void weatherDevSection(Weather& weather)
    {
        if (!ImGui::TreeNode("Weather"))
        {
            return;
        }

        bool isRaining = weather.kind == WeatherKind::Rain;

        if (ImGui::Checkbox("Rain", &isRaining))
        {
            weather.kind = isRaining ? WeatherKind::Rain : WeatherKind::Clear;
        }

        WeatherTuning& tuning = weather.tuning;
        ImGui::SliderFloat("Intensity", &tuning.intensity, 0.0f, 1.0f, "%.2f");
        ImGui::Text("Wetness %.2f, overcast look %.2f%s", weather.wetness, weather.lookAmount,
            weather.forced ? " (--weather)" : "");
        ImGui::Checkbox("Rain streaks", &tuning.particles);
        ImGui::SameLine();
        ImGui::Checkbox("Splashes", &tuning.splashes);
        ImGui::Checkbox("Wet surfaces", &tuning.wetness);
        ImGui::SameLine();
        ImGui::Checkbox("Overcast look", &tuning.overcastLook);
        ImGui::SameLine();
        ImGui::Checkbox("Sound", &tuning.sound);
        ImGui::SliderFloat("Streak density", &tuning.streakDensity, 0.0f, 3.0f, "%.2f");
        ImGui::SliderFloat("Splashes per second", &tuning.splashRate, 0.0f, 600.0f, "%.0f");
        ImGui::SliderFloat("Soak time (s)", &tuning.soakSeconds, 0.0f, 120.0f, "%.0f");
        ImGui::SliderFloat("Dry time (s)", &tuning.drySeconds, 0.0f, 600.0f, "%.0f");

        if (ImGui::Button("Soak now"))
        {
            weather.wetness = 1.0f;
        }

        ImGui::SameLine();

        if (ImGui::Button("Dry now"))
        {
            weather.wetness = 0.0f;
        }

        ImGui::TreePop();
    }
#endif
} //namespace game
