#include "Game.hpp"

#include "DevPanel.hpp"
#include "GameState.hpp"
#include "Simulation/HostSimulation.hpp"

#include <Lockstep/Net/Replication.hpp>
#include <Lockstep/Net/Session.hpp>
#include <Lockstep/Physics/PhysicsWorld.hpp>
#include <Lockstep/Platform/Input.hpp>
#include <Lockstep/Platform/Window.hpp>
#include <Lockstep/Render/Renderer.hpp>
#include <Lockstep/World/Entities.hpp>
#include <Lockstep/World/TimeDomains.hpp>

#include <iostream>
#include <memory>
#include <optional>
#include <vector>

namespace game
{
    namespace
    {
        namespace net = lockstep::net;
        namespace platform = lockstep::platform;
        namespace world = lockstep::world;

        using lockstep::app::EngineContext;
        using lockstep::app::FrameInfo;
        using lockstep::app::GameState;
        using lockstep::app::TickInfo;

        constexpr math::Vec3 k_lobbyEye{0.0f, 6.0f, 18.0f};
        constexpr math::Vec3 k_lobbyLook{0.0f, -0.3f, -1.0f};

        void logError(const char* what, const lockstep::Error& error)
        {
            std::clog << "Game: " << what << ": " << error.message << '\n';
        }

        lockstep::Result<GameState*> init(EngineContext& engine)
        {
            auto visuals = createVisuals(engine.renderer);

            if (!visuals)
            {
                return visuals.error();
            }

            auto state = std::make_unique<GameState>();
            state->visuals = visuals.value();
            applyArenaLook(engine.renderer);
            state->arena = buildArena();

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
            state.local.command = sampleLocalCommand(state.local.controls, state.settings.controls, tick.input,
                engine.window.isCursorCaptured());

            if (engine.session.role() == net::Role::Client)
            {
                receiveStatuses(state, engine);
                const auto encoded = encodeCommand(state.local.command);
                predictLocalPawn(state, engine, tick.tick, decodeCommand(encoded).value_or(state.local.command));

                if (auto sent = engine.replication.setLocalInput(encoded); !sent)
                {
                    logError("input not sent", sent.error());
                }

                return;
            }

            runHostTick(state, engine);
        }

        void updateCursorCapture(platform::Window& window, const platform::Input& input)
        {
            if (input.wasKeyPressed(platform::Key::Escape))
            {
                if (window.isCursorCaptured())
                {
                    window.setCursorCaptured(false);
                }
                else
                {
                    window.close();
                }
            }
            else if (input.wasMouseButtonPressed(platform::MouseButton::Left))
            {
                window.setCursorCaptured(true);
            }
        }

        void presentHost(GameState& state, EngineContext& engine, float alpha, std::optional<math::Transform>& localPawn)
        {
            const world::EntityId localEntity = state.simulation.pawns[k_hostPawn].entity;

            for (const Pawn& pawn : state.simulation.pawns)
            {
                const world::Entity* entity = engine.entities.find(pawn.entity);

                if (entity == nullptr)
                {
                    continue;
                }

                const math::Transform transform = entity->interpolatedTransform(alpha);

                if (pawn.entity == localEntity)
                {
                    localPawn = transform;
                    continue;
                }

                drawEntity(engine.renderer, state.visuals, Archetype::Pawn, entity->owner(), transform);
            }

            if (const world::Entity* enemy = engine.entities.find(state.simulation.enemy.entity))
            {
                drawEntity(engine.renderer, state.visuals, Archetype::Enemy, enemy->owner(),
                    enemy->interpolatedTransform(alpha));
            }
        }

        void presentClient(GameState& state, EngineContext& engine, float alpha, std::optional<math::Transform>& localPawn)
        {
            localPawn = predictedTransform(state.local.prediction, engine.entities, alpha);

            for (const net::MirroredEntity& mirrored : engine.replication.mirrored())
            {
                const world::Entity* entity = engine.entities.find(mirrored.entity);

                if (entity == nullptr)
                {
                    continue;
                }

                const auto archetype = static_cast<Archetype>(mirrored.archetype);
                const math::Transform transform = entity->interpolatedTransform(alpha);

                if (archetype == Archetype::Pawn && entity->owner() == world::EntityOwner::Player2)
                {
                    continue;
                }

                drawEntity(engine.renderer, state.visuals, archetype, entity->owner(), transform);
            }
        }

        void frame(GameState& state, EngineContext& engine, const FrameInfo& frameInfo)
        {
            updateCursorCapture(engine.window, frameInfo.input);
            drawArena(engine.renderer, state.visuals, state.arena);
            updateLighting(state.lighting, frameInfo.input, frameInfo.frameSeconds);
            submitArenaLights(engine.renderer, state.lighting);

            std::optional<math::Transform> localPawn;

            if (engine.session.role() == net::Role::Client)
            {
                presentClient(state, engine, frameInfo.alpha, localPawn);
            }
            else
            {
                presentHost(state, engine, frameInfo.alpha, localPawn);
            }

            const bool isClient = engine.session.role() == net::Role::Client;
            const PlayerStatus& status = state.local.statuses[isClient ? k_clientPawn : k_hostPawn];

            if (!localPawn)
            {
                const math::Vec3 lobbyDirection = math::normalize(k_lobbyLook);
                engine.renderer.setCamera(firstPersonCamera(k_lobbyEye, lobbyDirection));
                submitLocalFlashlight(engine.renderer, state.lighting, k_lobbyEye, lobbyDirection);

                if (status.isJoined)
                {
                    drawHealth(engine.renderer, state.visuals, k_lobbyEye, lobbyDirection, status,
                        state.settings.pawns.maxHealth);
                }

                return;
            }

            const math::Vec3 eye = eyePosition(*localPawn);
            const math::Vec3 direction = lookDirection(state.local.command.yaw, state.local.command.pitch);
            engine.renderer.setCamera(firstPersonCamera(eye, direction));
            submitLocalFlashlight(engine.renderer, state.lighting, eye, direction);
            drawCrosshair(engine.renderer, state.visuals, eye, direction);

            if (status.isJoined)
            {
                drawHealth(engine.renderer, state.visuals, eye, direction, status, state.settings.pawns.maxHealth);
            }
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
