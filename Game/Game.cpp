#include "Game.hpp"

#include "DevPanel.hpp"
#include "GameState.hpp"
#include "Player/LocalPlayer.hpp"
#include "Presentation/Frame.hpp"
#include "Simulation/HospitalSimulation.hpp"
#include "Simulation/HostSimulation.hpp"
#include "Simulation/LevelFlow.hpp"
#include "Simulation/NpcSimulation.hpp"
#include "Simulation/RoomSimulation.hpp"
#include "Simulation/StationSimulation.hpp"

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

        void matchStridesToCharacters(GameState& state)
        {
            if (!state.pawnCharacters)
            {
                return;
            }

            const StrideLengths strides = pawnStrideLengths(*state.pawnCharacters);
            HeadMotionTuning& tuning = state.headMotion.tuning;
            tuning.walkStride = strides.walk > 0.0f ? strides.walk : tuning.walkStride;
            tuning.runStride = strides.run > 0.0f ? strides.run : tuning.runStride;
            tuning.crouchStride = strides.crouch > 0.0f ? strides.crouch : tuning.crouchStride;
            std::clog << "Game: stride cycles from the character clips (m): walk " << tuning.walkStride << ", run "
                      << tuning.runStride << ", crouch " << tuning.crouchStride << '\n';
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
            matchStridesToCharacters(*state);
            loadFootsteps(state->footsteps, engine.audio, engine.files);
            applyArenaLook(engine.renderer);

            if (auto loaded = startLevel(*state, engine, launch.value()); !loaded)
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

        void receiveStatuses(GameState& state, EngineContext& engine)
        {
            for (const std::vector<std::byte>& message : engine.session.received())
            {
                if (receiveLevelMessage(state, engine, message) || receiveRoomMessage(state, message)
                    || receiveStationMessage(state, message) || receiveHospitalMessage(state, message)
                    || receiveNpcMessage(state, engine, message))
                {
                    continue;
                }

                if (const std::optional<StatusMessage> status = decodeStatusMessage(message))
                {
                    const PlayerStatus& before = state.local.statuses[k_clientPawn];
                    const PlayerStatus& after = status->players[k_clientPawn];

                    if (after.isDowned && !before.isDowned)
                    {
                        std::clog << "Game: you are down\n";
                    }
                    else if (after.health != before.health)
                    {
                        std::clog << "Game: health " << after.health << '\n';
                    }

                    if (status->isCombatActive != state.local.isCombatActive)
                    {
                        std::clog << "Game: combat " << (status->isCombatActive ? "started" : "ended") << '\n';
                    }

                    state.local.statuses = status->players;
                    state.local.isCombatActive = status->isCombatActive;
                    state.local.isRestarting = status->isRestarting;
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
            const PlayerStatus& own = state.local.statuses[localPawnIndex(engine)];
            state.local.command = sampleLocalCommand(state.local.controls, state.settings.controls,
                isFlying ? idle : tick.input, !isFlying && engine.window.isCursorCaptured(),
                WeaponControl{.isArmed = own.isArmed && !own.isDowned,
                    .zone = state.local.weaponZone,
                    .isHoldingToy = isHoldingToy(state, engine),
                    .isReady = isWeaponReady(localPawnMotion(state, engine)),
                    .hasRounds = own.magazine > 0 && !own.isReloading});

            if (own.isDowned)
            {
                restrictDownedCommand(state.local.command, state.settings.downed.crawlScale);
            }

            if (engine.session.role() == net::Role::Client)
            {
                receiveStatuses(state, engine);
                const auto encoded = encodeCommand(state.local.command);
                PlayerCommand predicted = decodeCommand(encoded).value_or(state.local.command);

                if (state.local.statuses[k_clientPawn].isDowned)
                {
                    restrictDownedCommand(predicted, state.settings.downed.crawlScale);
                }

                predictLocalPawn(state, engine, tick.tick, predicted);

                if (auto sent = engine.replication.setLocalInput(encoded); !sent)
                {
                    logError("input not sent", sent.error());
                }

                runRoomClientTick(state, engine);
                runStationClientTick(state, engine);
                runHospitalClientTick(state, engine);
                presentLevel(state, engine);

                return;
            }

            runHostTick(state, engine);
            updateLevelFlowAsHost(state, engine);
            presentLevel(state, engine);
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
