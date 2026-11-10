#include "Shell/GameSession.hpp"

#include "GameState.hpp"
#include "Simulation/HostSimulation.hpp"
#include "Simulation/LevelFlow.hpp"
#include "Simulation/NpcSimulation.hpp"

#include <Lockstep/Assets/FileSystem.hpp>
#include <Lockstep/Audio/AudioSystem.hpp>
#include <Lockstep/Net/Session.hpp>
#include <Lockstep/Physics/PhysicsWorld.hpp>
#include <Lockstep/Platform/Window.hpp>
#include <Lockstep/Render/Renderer.hpp>
#include <Lockstep/World/Entities.hpp>
#include <Lockstep/World/TimeDomains.hpp>

#include <algorithm>
#include <iostream>
#include <string>
#include <vector>

namespace game
{
    namespace
    {
        namespace audio = lockstep::audio;
        namespace net = lockstep::net;
        namespace world = lockstep::world;

        using lockstep::app::EngineContext;
        using lockstep::app::GameState;

        constexpr std::string_view k_armedFlag{"--armed"};
        constexpr std::string_view k_ambienceFolder{"audio/"};
        constexpr std::string_view k_ambienceName{"ambience_morning."};

        bool hasArgument(std::span<const char* const> arguments, std::string_view flag)
        {
            return std::any_of(arguments.begin(), arguments.end(), [flag](const char* argument)
            {
                return argument != nullptr && std::string_view(argument) == flag;
            });
        }

        std::optional<audio::VoiceHandle> startAmbience(EngineContext& engine)
        {
            for (const std::string& path : engine.files.list(""))
            {
                const std::string_view name = std::string_view(path).substr(path.rfind('/') + 1);

                if (!name.starts_with(k_ambienceName) || path.find(k_ambienceFolder) == std::string::npos)
                {
                    continue;
                }

                auto sound = engine.audio.loadSound(engine.files, path,
                    audio::SoundDescription{.is3d = false, .isLooping = true, .isStreamed = true});

                if (!sound)
                {
                    std::clog << "Game: loading ambience: " << sound.error().message << '\n';
                    return std::nullopt;
                }

                auto voice = engine.audio.play(sound.value(), {.bus = audio::Bus::Ambience});

                if (!voice)
                {
                    std::clog << "Game: playing ambience: " << voice.error().message << '\n';
                    return std::nullopt;
                }

                std::clog << "Game: playing ambience '" << path << "'\n";
                return voice.value();
            }

            return std::nullopt;
        }

        void stopVoice(EngineContext& engine, std::optional<audio::VoiceHandle>& voice)
        {
            if (voice)
            {
                engine.audio.stop(*voice);
                voice.reset();
            }
        }

        void stopGameSounds(GameState& state, EngineContext& engine)
        {
            CombatAudio& combat = state.combatAudio;
            stopVoice(engine, combat.heartbeat);
            stopVoice(engine, combat.street);
            stopVoice(engine, combat.hum);
            stopVoice(engine, combat.rain);
            stopVoice(engine, combat.rainIndoor);
            stopVoice(engine, state.shell.gameAmbience);
            presentLevelLights(state.lightAnimation, engine, nullptr, math::Vec3{0.0f, 0.0f, 0.0f}, 0.0f);
        }

        void removeHostObjects(GameState& state, EngineContext& engine)
        {
            for (Pawn& pawn : state.simulation.pawns)
            {
                if (!pawn.entity.isNull())
                {
                    despawnPawn(pawn, engine.entities, engine.physics);
                }
            }

            Enemy& enemy = state.simulation.enemy;

            if (!enemy.controller.isNull())
            {
                engine.physics.destroyController(enemy.controller);
            }

            if (!enemy.entity.isNull())
            {
                engine.entities.destroy(enemy.entity);
            }

            clearLevelNpcs(state, engine);
        }

        void destroyRemainingEntities(EngineContext& engine)
        {
            const std::vector<world::EntityId> ids(engine.entities.ids().begin(), engine.entities.ids().end());

            for (const world::EntityId id : ids)
            {
                engine.entities.destroy(id);
            }

            if (!ids.empty())
            {
                std::clog << "Game: removed " << ids.size() << " leftover entities\n";
            }
        }

        void resetGameState(GameState& state)
        {
            const bool showSubtitles = state.hud.showSubtitles;
            state.simulation = GameState::Simulation{};
            state.local = GameState::Local{};
            state.hud = Hud{};
            state.hud.showSubtitles = showSubtitles;
            state.weaponZones.clear();
            state.characters = SceneCharacters{};
            state.npcs = NpcPlaceholders{};
            state.hospital.reset();
            state.echo.reset();
            state.flyCamera = FlyCamera{};
            state.subtitles = Subtitles{};
            state.flashlights = FlashlightState{};
            resetHeadMotion(state.headMotion);
        }
    } //namespace

    lockstep::Status startGame(GameState& state, EngineContext& engine, const std::optional<SceneLaunch>& launch)
    {
        state.settings.startArmed = hasArgument(engine.arguments, k_armedFlag) || !launch;

        if (auto loaded = startLevel(state, engine, launch); !loaded)
        {
            return loaded;
        }

        if (!state.intro)
        {
            state.shell.gameAmbience = startAmbience(engine);
        }

        if (engine.session.role() == net::Role::Client)
        {
            return createArenaBodies(state.arena, engine.physics);
        }

        return startHostSimulation(state, engine);
    }

    void endGame(GameState& state, EngineContext& engine)
    {
        std::clog << "Game: leaving the game\n";

        if (engine.session.role() != net::Role::Client)
        {
            removeHostObjects(state, engine);
        }

        clearPrediction(state.local.prediction, engine.entities, engine.physics);

        for (const lockstep::physics::BodyId body : state.arena.bodies)
        {
            engine.physics.destroyBody(body);
        }

        state.arena = Arena{};
        unloadLevel(state, engine);
        beginNpcLooks(state.npcLooks);
        endNpcLooks(state.npcLooks, engine.physics, engine.renderer, 0.0f, 0.0f);
        stopGameSounds(state, engine);
        engine.session.disconnect();
        destroyRemainingEntities(engine);
        engine.timeDomains.setScale(world::TimeDomain::World, 1.0f);
        engine.renderer.setScreenFade(0.0f);
        engine.window.setCursorCaptured(false);
        resetGameState(state);

        const lockstep::physics::PhysicsStats physics = engine.physics.stats();
        std::clog << "Game: after leaving: " << physics.bodyCount << " bodies, " << physics.controllerCount
                  << " controllers, " << physics.ragdollCount << " ragdolls, " << engine.entities.count()
                  << " entities\n";
    }
} //namespace game
