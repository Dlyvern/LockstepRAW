#include "Shell/MenuBackdrop.hpp"

#include "Effects/Effects.hpp"
#include "GameState.hpp"
#include "Lights/LightAnimation.hpp"
#include "Presentation/LevelLook.hpp"
#include "Simulation/LevelFlow.hpp"

#include <Lockstep/Assets/FileSystem.hpp>
#include <Lockstep/Assets/Scene.hpp>
#include <Lockstep/Audio/AudioSystem.hpp>
#include <Lockstep/Render/Renderer.hpp>
#include <Lockstep/World/TimeDomains.hpp>

#include <cmath>
#include <iostream>
#include <string>
#include <string_view>

namespace game
{
    namespace
    {
        namespace audio = lockstep::audio;
        namespace render = lockstep::render;

        using lockstep::app::EngineContext;
        using lockstep::app::GameState;

        constexpr std::string_view k_backdropLevel{"levels/hospital_a/scene"};
        constexpr std::string_view k_backdropName{"hospital_a"};
        constexpr std::string_view k_startNode{"PlayerStart_1"};
        constexpr float k_eyeHeight{1.65f};
        constexpr float k_lookDistance{25.0f};
        constexpr float k_rainVolume{0.55f};
        constexpr float k_sirenVolume{0.35f};
        constexpr float k_streetVolume{0.25f};

        std::optional<audio::VoiceHandle> playLoop(EngineContext& engine, std::string_view path, float volume)
        {
            auto sound = engine.audio.loadSound(engine.files, path,
                audio::SoundDescription{.is3d = false, .isLooping = true, .isStreamed = true});

            if (!sound)
            {
                std::clog << "Game: menu ambience '" << path << "' not loaded: " << sound.error().message << '\n';
                return std::nullopt;
            }

            auto voice = engine.audio.play(sound.value(), {.bus = audio::Bus::Ambience, .volume = volume});

            if (!voice)
            {
                return std::nullopt;
            }

            return voice.value();
        }

        void stopLoop(EngineContext& engine, std::optional<audio::VoiceHandle>& voice)
        {
            if (voice)
            {
                engine.audio.stop(*voice);
                voice.reset();
            }
        }

        void placeCamera(MenuBackdrop& backdrop, const lockstep::assets::Scene& scene)
        {
            const std::optional<math::Transform> start = lockstep::assets::findNode(scene, k_startNode);

            if (!start)
            {
                return;
            }

            const math::Vec3 facing = start->rotation * math::Vec3{0.0f, 0.0f, -1.0f};
            const math::Vec3 flat{facing.x, 0.0f, facing.z};
            backdrop.forward = math::length(flat) > 0.01f ? math::normalize(flat) : math::Vec3{0.0f, 0.0f, -1.0f};
            backdrop.anchor = start->position + math::Vec3{0.0f, k_eyeHeight, 0.0f};
        }

        render::Camera driftingCamera(const MenuBackdrop& backdrop)
        {
            const float time = backdrop.seconds;
            const math::Vec3 right = math::normalize(math::cross(backdrop.forward, math::Vec3{0.0f, 1.0f, 0.0f}));
            const math::Vec3 eye = backdrop.anchor + right * (std::sin(time * 0.045f) * 1.1f)
                + backdrop.forward * (std::sin(time * 0.031f) * 0.7f - 0.6f)
                + math::Vec3{0.0f, std::sin(time * 0.07f) * 0.06f, 0.0f};
            const float yaw = std::sin(time * 0.023f) * 0.14f;
            const math::Vec3 turned = backdrop.forward * std::cos(yaw) + right * std::sin(yaw);
            const math::Vec3 look = turned * k_lookDistance + math::Vec3{0.0f, 1.2f + std::sin(time * 0.017f) * 0.4f, 0.0f};

            return render::Camera{.position = eye, .target = eye + look, .up = {0.0f, 1.0f, 0.0f}};
        }
    } //namespace

    void loadMenuBackdrop(GameState& state, EngineContext& engine)
    {
        MenuBackdrop& backdrop = state.shell.backdrop;
        backdrop.seconds = 0.0f;

        if (!engine.files.exists(k_backdropLevel))
        {
            std::clog << "Game: menu background '" << k_backdropLevel << "' is not mounted (dark menu)\n";
            return;
        }

        auto level = loadSceneLevel(SceneLaunch{.level = std::string(k_backdropLevel), .path = {}, .scale = 1.0f},
            engine.renderer, engine.physics, engine.files);

        if (!level)
        {
            std::clog << "Game: menu background not loaded: " << level.error().message << '\n';
            return;
        }

        state.level = std::move(level).value();
        backdrop.isLoaded = true;
        backdrop.savedWeather = state.weather.forced;
        state.weather.forced = WeatherKind::Rain;
        state.weather.scene = nullptr;
        applyLookPreset(engine.renderer.settings(), LookPreset::Sunset);
        state.npcs = findNpcPlaceholders(state.level->scene, k_backdropName);
        placeCamera(backdrop, state.level->scene);
        backdrop.rain = playLoop(engine, "sounds/ambience/rain_outdoor.ogg", k_rainVolume);
        backdrop.siren = playLoop(engine, "sounds/ambience/siren_distant.ogg", k_sirenVolume);
        backdrop.street = playLoop(engine, "sounds/ambience/street_loop.ogg", k_streetVolume);
    }

    void unloadMenuBackdrop(GameState& state, EngineContext& engine)
    {
        MenuBackdrop& backdrop = state.shell.backdrop;
        stopLoop(engine, backdrop.rain);
        stopLoop(engine, backdrop.siren);
        stopLoop(engine, backdrop.street);
        presentLevelLights(state.lightAnimation, engine, nullptr, backdrop.anchor, 0.0f);

        if (!backdrop.isLoaded)
        {
            return;
        }

        unloadLevel(state, engine);
        state.npcs = NpcPlaceholders{};
        state.weather.forced = backdrop.savedWeather;
        state.weather.scene = nullptr;
        backdrop.isLoaded = false;
    }

    void presentMenuBackdrop(GameState& state, EngineContext& engine, float frameSeconds)
    {
        MenuBackdrop& backdrop = state.shell.backdrop;
        backdrop.seconds += frameSeconds;
        const render::Camera camera = driftingCamera(backdrop);
        engine.renderer.setCamera(camera);

        if (!backdrop.isLoaded || !state.level)
        {
            return;
        }

        const float worldSeconds = frameSeconds;
        lockstep::assets::submitScene(engine.renderer, state.level->scene);
        presentWeather(state, engine, WeatherView{.eye = camera.position,
            .forward = lockstep::math::normalize(camera.target - camera.position), .playerSeconds = 0.0f},
            worldSeconds, frameSeconds);
        presentEffects(state, engine, camera.position, worldSeconds, frameSeconds);
        presentLevelLights(state.lightAnimation, engine, &state.level->scene, camera.position, worldSeconds);

        if (state.pawnCharacters)
        {
            beginPawnCharacters(*state.pawnCharacters);
        }

        drawNpcPlaceholders(engine.renderer, &engine.physics, state.npcs,
            state.pawnCharacters ? &*state.pawnCharacters : nullptr, frameSeconds);
    }
} //namespace game
