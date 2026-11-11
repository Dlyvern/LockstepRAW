#include "Game.hpp"

#include "Breakables/GlassDevTools.hpp"
#include "Breakables/GlassReplication.hpp"
#include "DevPanel.hpp"
#include "GameState.hpp"
#include "Intro/Intro.hpp"
#include "Npcs/NpcRoster.hpp"
#include "Player/FireDrill.hpp"
#include "Player/WeaponDrill.hpp"
#include "Player/LocalPlayer.hpp"
#include "Player/MoveDrill.hpp"
#include "Presentation/Frame.hpp"
#include "Shell/GameSession.hpp"
#include "Shell/Shell.hpp"
#include "Shell/ShellLaunch.hpp"
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
#include <utility>
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

        constexpr std::string_view k_npcCrouchFlag{"--dev-npc-crouch"};
        constexpr std::string_view k_bloodyNpcsFlag{"--dev-bloody-npcs"};
        constexpr std::string_view k_shakeClothFlag{"--dev-shake-cloth"};
        constexpr std::string_view k_lightningDrillFlag{"--dev-lightning-drill"};
        constexpr std::string_view k_ironSightsFlag{"--dev-iron-sights"};
        constexpr std::string_view k_locomotionOffFlag{"--dev-locomotion-off"};
        constexpr std::string_view k_motionMatchingOffFlag{"--dev-motion-matching-off"};
        constexpr uint32_t k_menuFrameRateCap{60};
        constexpr float k_drillTargetRange{6.0f};
        constexpr float k_watchRange{30.0f};
        constexpr float k_watchEyeHeight{1.6f};
        constexpr float k_watchTargetHeight{0.2f};

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
            tuning.walkStrideSpeed = strides.walkSpeed > 0.0f ? strides.walkSpeed : tuning.walkStrideSpeed;
            tuning.runStrideSpeed = strides.runSpeed > strides.walkSpeed ? strides.runSpeed : tuning.runStrideSpeed;
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

            auto weather = parseWeatherFlag(engine.arguments);

            if (!weather)
            {
                return weather.error();
            }

            auto state = std::make_unique<GameState>();
            state->visuals = visuals.value();
            state->weather.forced = weather.value();
            state->npcLooks.isCrouchForced = hasArgument(engine.arguments, k_npcCrouchFlag);
            state->gore.stains.drill = hasArgument(engine.arguments, k_bloodyNpcsFlag);
            state->curtains.tuning.shakeDrill = hasArgument(engine.arguments, k_shakeClothFlag);

            if (hasArgument(engine.arguments, k_lightningDrillFlag))
            {
                startLightningDrill(state->weather);
            }
            state->pawnCharacters = loadPawnCharacters(engine.renderer, engine.files);

            if (state->pawnCharacters && hasArgument(engine.arguments, k_locomotionOffFlag))
            {
                disableLocomotionLayers(state->pawnCharacters->locomotionTuning);
            }

            if (state->pawnCharacters && hasArgument(engine.arguments, k_motionMatchingOffFlag))
            {
                state->pawnCharacters->locomotionTuning.isMotionMatched = false;
            }

            state->optics = loadOptics(engine.renderer, engine.files);

            if (hasArgument(engine.arguments, k_ironSightsFlag))
            {
                state->optics.loadout.byWeapon.fill(game::OpticKind::None);
            }
            matchStridesToCharacters(*state);
            loadFootsteps(state->footsteps, engine.audio, engine.files);
            state->combatAudio.bank = loadCombatSounds(engine.audio, engine.files);
            loadWeaponFx(state->weaponFx, engine);
            state->devCapture = parseDevCapture(engine.arguments);
            state->materialDrill = parseMaterialTimingDrill(engine.arguments);
            state->weaponFeedback.sounds = loadHandlingSounds(engine.audio, engine.files);
            applyArenaLook(engine.renderer);

            initShell(*state, engine);

            if (isMenuLaunch(engine.arguments))
            {
                enterMenu(*state, engine);
                auto intro = startIntro(engine);

                if (!intro)
                {
                    return intro.error();
                }

                state->intro = std::move(intro).value();
                return state.release();
            }

            if (auto started = startGame(*state, engine, launch.value()); !started)
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
                    || receiveNpcMessage(state, engine, message) || receiveGoreMessage(state, message)
                    || receiveKickMessage(state.kicks, message)
                    || receiveGlassMessage(state.glass, state.level ? &*state.level : nullptr, engine.physics, message))
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

        std::optional<FireButtons> scriptedFire(GameState& state, const EngineContext& engine, uint64_t tick)
        {
#if LOCKSTEP_DEV_TOOLS
            applyWeaponDrill(state.local.controls, state.settings.weapons, engine.arguments, tick,
                engine.timeDomains.delta(world::TimeDomain::Player));

            if (isFireDrillRequested(engine.arguments))
            {
                return fireDrillButtons(state.local.controls, tick, engine.timeDomains.delta(world::TimeDomain::Player));
            }

            if (isScopeDrillRequested(engine.arguments))
            {
                return scopeDrillButtons(state.local.controls, tick, engine.timeDomains.delta(world::TimeDomain::Player));
            }
#else
            static_cast<void>(state);
            static_cast<void>(engine);
            static_cast<void>(tick);
#endif
            return std::nullopt;
        }

#if LOCKSTEP_DEV_TOOLS
        std::optional<math::Vec3> nearestNpc(const GameState& state, const EngineContext& engine,
            const math::Vec3& foot, float range)
        {
            std::optional<math::Vec3> nearest;

            for (const Npc& npc : state.simulation.npcs.npcs)
            {
                const world::Entity* entity = engine.entities.find(npc.entity);

                if (entity == nullptr || !isNpcAlive(npc))
                {
                    continue;
                }

                const math::Vec3 position = entity->transform().position;
                const float distance = math::length(position - foot);

                if (distance < range && (!nearest || distance < math::length(*nearest - foot)))
                {
                    nearest = position;
                }
            }

            return nearest;
        }
#endif

        void fixedTick(GameState& state, EngineContext& engine, const TickInfo& tick)
        {
            if (state.intro || tickShell(state, engine, tick))
            {
                return;
            }

            state.lastTick = tick.tick;
            stepEcho(state);
            const platform::Input idle{};
            const bool isFlying = state.flyCamera.isActive;
            const bool isInputIdle = isFlying || isShellBlockingInput(state.shell);
            state.local.weaponZone = localWeaponZone(state, engine);
            const PlayerStatus& own = state.local.statuses[localPawnIndex(engine)];
            const PlayerStatus& partner = state.local.statuses[localPawnIndex(engine) == k_hostPawn ? k_clientPawn
                                                                                                   : k_hostPawn];
            state.local.command = sampleLocalCommand(state.local.controls, state.settings.controls,
                isInputIdle ? idle : tick.input, !isFlying && engine.window.isCursorCaptured(),
                WeaponControl{.isArmed = own.isArmed && !own.isDowned,
                    .zone = state.local.weaponZone,
                    .isHoldingToy = isHoldingToy(state, engine),
                    .isReady = isWeaponReady(localPawnMotion(state, engine)),
                    .magazine = own.magazine,
                    .isReloading = own.isReloading,
                    .weapons = &state.settings.weapons,
                    .aimOffset = state.local.aimOffset,
                    .canToggleFlashlight = state.hud.prompt.empty() && !partner.isDowned,
                    .scriptedFire = scriptedFire(state, engine, tick.tick)},
                engine.timeDomains.delta(world::TimeDomain::Player));

            const OpticModel* optic = own.isArmed ? weaponOptic(state.optics, state.local.command.weapon) : nullptr;
            state.local.command.optic = optic ? optic->kind : OpticKind::None;

            if (own.isDowned)
            {
                restrictDownedCommand(state.local.command, state.settings.downed.crawlScale);
            }

#if LOCKSTEP_DEV_TOOLS
            if (isWatchNpcRequested(engine.arguments))
            {
                if (const auto foot = engine.physics.footPosition(localController(state, engine)))
                {
                    if (const std::optional<math::Vec3> npc = nearestNpc(state, engine, *foot, k_watchRange))
                    {
                        lookAtPoint(state.local.command, *foot + math::Vec3{0.0f, k_watchEyeHeight, 0.0f},
                            *npc + math::Vec3{0.0f, k_watchTargetHeight, 0.0f});
                    }
                }
            }

            if (const std::optional<float> pitch = devLookPitch(engine.arguments))
            {
                state.local.command.pitch = *pitch;
            }

            if (isKickDrillRequested(engine.arguments))
            {
                if (const auto foot = engine.physics.footPosition(localController(state, engine)))
                {
                    applyKickDrill(state.local.command, tick.tick, engine.timeDomains.delta(world::TimeDomain::Player),
                        *foot, nearestNpc(state, engine, *foot, k_drillTargetRange));
                }
            }

            if (isVaultDrillRequested(engine.arguments))
            {
                const float delta = engine.timeDomains.delta(world::TimeDomain::Player);

                if (const auto foot = engine.physics.footPosition(localController(state, engine)))
                {
                    applyVaultDrill(state.local.command, *foot, tick.tick, delta);
                }

                if (engine.session.role() == net::Role::Client)
                {
                    logMoveDrill(state.local.prediction, tick.tick, delta);
                }
            }
            else if (isMoveDrillRequested(engine.arguments))
            {
                const float delta = engine.timeDomains.delta(world::TimeDomain::Player);
                applyMoveDrill(state.local.command, tick.tick, delta);

                if (engine.session.role() == net::Role::Client)
                {
                    logMoveDrill(state.local.prediction, tick.tick, delta);
                }
            }
#endif

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
#if LOCKSTEP_DEV_TOOLS
            updateGlassDevBreaks(state, engine);
#endif
            publishGlass(state.glass, engine.session);
            updateLevelFlowAsHost(state, engine);
            presentLevel(state, engine);
        }

        void frame(GameState& state, EngineContext& engine, const lockstep::app::FrameInfo& frameInfo)
        {
            engine.renderer.setFrameRateCap(state.intro || state.shell.isInMenu ? k_menuFrameRateCap : 0);

            if (state.intro)
            {
                updateDevCapture(state.devCapture, engine.renderer, DevCaptureEvents{});

                if (presentIntro(*state.intro, engine, frameInfo))
                {
                    return;
                }

                state.intro.reset();
                return;
            }

            presentShell(state, engine, frameInfo);
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
        api.frame = &frame;
        api.shutdown = &shutdown;

#if LOCKSTEP_DEV_TOOLS
        api.devToolsPanels = &devToolsPanels;
#endif

        return api;
    }
} //namespace game
