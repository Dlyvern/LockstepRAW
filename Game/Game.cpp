#include "Game.hpp"

#include "DevPanel.hpp"
#include "GameState.hpp"
#include "Player/LocalPlayer.hpp"
#include "Presentation/Frame.hpp"
#include "Simulation/HostSimulation.hpp"
#include "Simulation/RoomSimulation.hpp"

#include <Lockstep/Assets/FileSystem.hpp>
#include <Lockstep/Assets/Scene.hpp>
#include <Lockstep/Audio/AudioSystem.hpp>
#include <Lockstep/Net/Replication.hpp>
#include <Lockstep/Net/Session.hpp>
#include <Lockstep/Physics/PhysicsWorld.hpp>
#include <Lockstep/Platform/Input.hpp>
#include <Lockstep/Platform/Window.hpp>
#include <Lockstep/Render/Renderer.hpp>
#include <Lockstep/World/Entities.hpp>
#include <Lockstep/World/TimeDomains.hpp>

#include <algorithm>
#include <iostream>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace game
{
    namespace
    {
        namespace net = lockstep::net;
        namespace platform = lockstep::platform;
        namespace world = lockstep::world;

        using lockstep::app::EngineContext;
        using lockstep::app::GameState;
        using lockstep::app::TickInfo;

        constexpr std::string_view k_armedFlag{"--armed"};
        constexpr std::string_view k_ambienceFolder{"audio/"};
        constexpr std::string_view k_ambienceName{"ambience_morning."};

        void logError(const char* what, const lockstep::Error& error)
        {
            std::clog << "Game: " << what << ": " << error.message << '\n';
        }

        bool hasArgument(std::span<const char* const> arguments, std::string_view flag)
        {
            return std::any_of(arguments.begin(), arguments.end(), [flag](const char* argument)
            {
                return argument != nullptr && std::string_view(argument) == flag;
            });
        }

        lockstep::Status loadLevel(GameState& state, EngineContext& engine, const std::optional<SceneLaunch>& launch)
        {
            if (!launch)
            {
                state.arena = buildArena();
                return startRoom(state, engine, LevelSpawns{});
            }

            auto level = loadSceneLevel(*launch, engine.renderer, engine.physics, engine.files);

            if (!level)
            {
                return level.error();
            }

            state.level = std::move(level).value();
            state.weaponZones = findWeaponZones(state.level->scene);
            state.characters = createSceneCharacters(state.level->scene, engine.arguments);
            const LevelSpawns spawns = findLevelSpawns(*state.level, engine.physics);
            state.settings.spawnPoints = spawns.pawns;
            state.settings.enemy.spawnPoint = spawns.enemy;

            return startRoom(state, engine, spawns);
        }

        void startAmbience(EngineContext& engine)
        {
            for (const std::string& path : engine.files.list(""))
            {
                const std::string_view name = std::string_view(path).substr(path.rfind('/') + 1);

                if (!name.starts_with(k_ambienceName) || path.find(k_ambienceFolder) == std::string::npos)
                {
                    continue;
                }

                auto sound = engine.audio.loadSound(engine.files, path,
                    lockstep::audio::SoundDescription{.is3d = false, .isLooping = true, .isStreamed = true});

                if (!sound)
                {
                    logError("loading ambience", sound.error());
                    return;
                }

                if (auto voice = engine.audio.play(sound.value(), {.bus = lockstep::audio::Bus::Ambience}); !voice)
                {
                    logError("playing ambience", voice.error());
                    return;
                }

                std::clog << "Game: playing ambience '" << path << "'\n";
                return;
            }
        }

        lockstep::Result<GameState*> init(EngineContext& engine)
        {
            auto visuals = createVisuals(engine.renderer);

            if (!visuals)
            {
                return visuals.error();
            }

            auto launch = parseSceneLaunch(engine.arguments);

            if (!launch)
            {
                return launch.error();
            }

            auto state = std::make_unique<GameState>();
            state->visuals = visuals.value();
            state->settings.startArmed = hasArgument(engine.arguments, k_armedFlag) || !launch.value();
            state->pawnCharacters = loadPawnCharacters(engine.renderer, engine.files);
            applyArenaLook(engine.renderer);

            if (auto loaded = loadLevel(*state, engine, launch.value()); !loaded)
            {
                return loaded.error();
            }

            startAmbience(engine);

            if (engine.session.role() == net::Role::Client)
            {
                if (auto created = createArenaBodies(state->arena, engine.physics); !created)
                {
                    return created.error();
                }

                return state.release();
            }

            if (auto started = startHostSimulation(*state, engine); !started)
            {
                return started.error();
            }

            return state.release();
        }

        void receiveStatuses(GameState& state, const EngineContext& engine)
        {
            for (const std::vector<std::byte>& message : engine.session.received())
            {
                if (receiveRoomMessage(state, message))
                {
                    continue;
                }

                if (const std::optional<PlayerStatuses> statuses = decodePlayerStatuses(message))
                {
                    const PlayerStatus& before = state.local.statuses[k_clientPawn];
                    const PlayerStatus& after = (*statuses)[k_clientPawn];

                    if (after.isDead && !before.isDead)
                    {
                        std::clog << "Game: you died\n";
                    }
                    else if (after.health != before.health)
                    {
                        std::clog << "Game: health " << after.health << '\n';
                    }

                    state.local.statuses = *statuses;
                }
            }
        }

        void predictLocalPawn(GameState& state, EngineContext& engine, uint64_t tick, const PlayerCommand& command)
        {
            Prediction& prediction = state.local.prediction;

            if (engine.session.state() != net::ConnectionState::Connected)
            {
                clearPrediction(prediction, engine.entities, engine.physics);
                return;
            }

            const float delta = engine.timeDomains.delta(world::TimeDomain::Player);

            if (const std::optional<net::InputAck> ack = engine.replication.inputAck())
            {
                if (const std::optional<float> error = reconcilePrediction(prediction, engine.entities, engine.physics,
                        *ack, state.settings.pawns, delta))
                {
                    engine.replication.recordCorrection(*error);
                }
            }

            predictPawn(prediction, engine.entities, engine.physics, tick, command, state.settings.pawns, delta);
        }

        void fixedTick(GameState& state, EngineContext& engine, const TickInfo& tick)
        {
            state.lastTick = tick.tick;
            stepEcho(state);
            const platform::Input idle{};
            const bool isFlying = state.flyCamera.isActive;
            state.local.weaponZone = localWeaponZone(state, engine);
            state.local.command = sampleLocalCommand(state.local.controls, state.settings.controls,
                isFlying ? idle : tick.input, !isFlying && engine.window.isCursorCaptured(),
                WeaponControl{.isArmed = state.local.statuses[localPawnIndex(engine)].isArmed,
                    .zone = state.local.weaponZone,
                    .isHoldingToy = isHoldingToy(state, engine)});

            if (engine.session.role() == net::Role::Client)
            {
                receiveStatuses(state, engine);
                const auto encoded = encodeCommand(state.local.command);
                predictLocalPawn(state, engine, tick.tick, decodeCommand(encoded).value_or(state.local.command));

                if (auto sent = engine.replication.setLocalInput(encoded); !sent)
                {
                    logError("input not sent", sent.error());
                }

                runRoomClientTick(state, engine);

                return;
            }

            runHostTick(state, engine);
        }

        void shutdown(GameState* state)
        {
            std::unique_ptr<GameState> owned(state);
        }
    } //namespace

    lockstep::app::GameApi gameApi()
    {
        lockstep::app::GameApi api{};
        api.init = &init;
        api.fixedTick = &fixedTick;
        api.frame = &presentFrame;
        api.shutdown = &shutdown;

#if LOCKSTEP_DEV_TOOLS
        api.devToolsPanels = &devToolsPanels;
#endif

        return api;
    }
} //namespace game
