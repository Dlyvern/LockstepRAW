#include "Presentation/Frame.hpp"

#include "GameState.hpp"
#include "Player/LocalPlayer.hpp"
#include "Presentation/Hud.hpp"
#include "Presentation/Mirrors.hpp"
#include "Simulation/HostSimulation.hpp"

#include <Lockstep/Net/Replication.hpp>
#include <Lockstep/Net/Session.hpp>
#include <Lockstep/Platform/Input.hpp>
#include <Lockstep/Platform/Window.hpp>
#include <Lockstep/Render/Renderer.hpp>
#include <Lockstep/World/Entities.hpp>
#include <Lockstep/World/TimeDomains.hpp>

#include <cmath>
#include <optional>
#include <utility>

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

        constexpr math::Vec3 k_lobbyEye{0.0f, 6.0f, 18.0f};
        constexpr math::Vec3 k_lobbyLook{0.0f, -0.3f, -1.0f};
        constexpr math::Vec3 k_levelLobbyOffset{0.0f, 3.0f, 4.0f};

        struct View
        {
            math::Vec3 eye{0.0f, 0.0f, 0.0f};
            math::Vec3 direction{0.0f, 0.0f, -1.0f};
        };

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

        PawnCharacters* pawnCharacters(GameState& state)
        {
            return state.pawnCharacters ? &*state.pawnCharacters : nullptr;
        }

        void presentHost(GameState& state, EngineContext& engine, float alpha, float playerSeconds,
            std::optional<math::Transform>& localPawn)
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

                drawPawn(engine.renderer, state.visuals, pawnCharacters(state), entity->owner(), transform,
                    remotePawnLook(state, entity->owner()), playerSeconds);
            }

            if (const world::Entity* enemy = engine.entities.find(state.simulation.enemy.entity))
            {
                drawEntity(engine.renderer, state.visuals, Archetype::Enemy, enemy->owner(),
                    enemy->interpolatedTransform(alpha));
            }
        }

        void presentClient(GameState& state, EngineContext& engine, float alpha, float playerSeconds,
            std::optional<math::Transform>& localPawn)
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

                if (archetype == Archetype::Pawn)
                {
                    if (entity->owner() != world::EntityOwner::Player2)
                    {
                        drawPawn(engine.renderer, state.visuals, pawnCharacters(state), entity->owner(), transform,
                            remotePawnLook(state, entity->owner()), playerSeconds);
                    }

                    continue;
                }

                drawEntity(engine.renderer, state.visuals, archetype, entity->owner(), transform);
            }
        }

        View lobbyView(const GameState& state)
        {
            if (!state.level)
            {
                return View{.eye = k_lobbyEye, .direction = math::normalize(k_lobbyLook)};
            }

            return View{.eye = state.settings.spawnPoints[k_hostPawn] + k_levelLobbyOffset,
                .direction = math::normalize(k_lobbyLook)};
        }

        bool presentFlyCamera(GameState& state, EngineContext& engine, const FrameInfo& frameInfo, const View& view)
        {
            const float yaw = std::atan2(-view.direction.x, -view.direction.z);
            const float pitch = std::asin(math::clamp(view.direction.y, -1.0f, 1.0f));
            toggleFlyCamera(state.flyCamera, frameInfo.input, view.eye, yaw, pitch);

            if (!state.flyCamera.isActive)
            {
                return false;
            }

            updateFlyCamera(state.flyCamera, frameInfo.input, state.settings.controls, engine.window.isCursorCaptured(),
                frameInfo.frameSeconds);
            engine.renderer.setCamera(flyCameraView(state.flyCamera));

            return true;
        }

        void presentToys(GameState& state, EngineContext& engine, float alpha, const std::optional<View>& view)
        {
            if (!state.room || !state.level)
            {
                return;
            }

            const ApartmentRoom& room = *state.room;
            const uint8_t held = room.state.heldToy[state.hud.localPlayer];
            std::optional<std::pair<std::size_t, math::Vec3>> localHeld;

            if (view && held < room.toys.size())
            {
                const ToyPlayer player{.eye = view->eye, .direction = view->direction,
                    .controller = localController(state, engine)};
                localHeld = std::pair<std::size_t, math::Vec3>{held, holdPoint(room.toys[held], engine.physics, player)};
            }

            drawToys(engine.renderer, state.level->scene, room.toys, engine.entities, alpha, localHeld);
        }

        void presentViewModel(GameState& state, EngineContext& engine, const std::optional<math::Vec3>& eye,
            float playerSeconds)
        {
            if (!state.pawnCharacters || !state.pawnCharacters->firstPerson)
            {
                return;
            }

            PawnCharacters& characters = *state.pawnCharacters;
            const PawnLook look = localPawnLook(state, engine);

            if (!eye || !look.isArmed || state.flyCamera.isActive)
            {
                hideViewModel(characters.viewModel);
                return;
            }

            const PlayerCommand& command = state.local.command;
            drawViewModel(engine.renderer, *characters.firstPerson, characters.viewModel,
                ViewModelInput{.eye = *eye,
                    .yaw = command.yaw,
                    .pitch = command.pitch,
                    .speed = pawnCharacterSpeed(characters, localOwner(engine)),
                    .walkSpeed = state.settings.pawns.walkSpeed,
                    .isAiming = look.isAiming,
                    .isLowered = look.isLowered,
                    .canFire = !isHoldingToy(state, engine),
                    .shotCount = command.shotCount,
                    .deltaSeconds = playerSeconds});
        }
    } //namespace

    void presentFrame(GameState& state, EngineContext& engine, const FrameInfo& frameInfo)
    {
        updateCursorCapture(engine.window, frameInfo.input);
        drawArena(engine.renderer, state.visuals, state.arena);

        if (state.level)
        {
            lockstep::assets::submitScene(engine.renderer, state.level->scene);
        }

        if (state.echo)
        {
            for (const std::optional<math::Transform>& ghost : echoGhostTransforms(*state.echo, frameInfo.alpha))
            {
                if (ghost)
                {
                    engine.renderer.submit(state.visuals.cube, state.visuals.ghost, *ghost);
                }
            }
        }

        updateLighting(state.lighting, frameInfo.input, frameInfo.frameSeconds);
        submitArenaLights(engine.renderer, state.lighting);

        std::optional<math::Transform> localPawn;
        const bool isClient = engine.session.role() == net::Role::Client;
        const float playerSeconds = frameInfo.frameSeconds * engine.timeDomains.scale(world::TimeDomain::Player);

        if (state.pawnCharacters)
        {
            beginPawnCharacters(*state.pawnCharacters);
        }

        if (isClient)
        {
            presentClient(state, engine, frameInfo.alpha, playerSeconds, localPawn);
        }
        else
        {
            presentHost(state, engine, frameInfo.alpha, playerSeconds, localPawn);
        }

        const PlayerStatus& status = state.local.statuses[isClient ? k_clientPawn : k_hostPawn];
        presentMirrors(engine.renderer, state.level ? &state.level->scene : nullptr);

        if (localPawn)
        {
            drawPawn(engine.renderer, state.visuals, pawnCharacters(state), localOwner(engine), *localPawn,
                localPawnLook(state, engine), playerSeconds, k_mirrorOnlyLayer);
        }

        const View view = localPawn
            ? View{.eye = eyePosition(*localPawn),
                .direction = lookDirection(state.local.command.yaw, state.local.command.pitch)}
            : lobbyView(state);
        presentToys(state, engine, frameInfo.alpha, localPawn ? std::optional<View>(view) : std::nullopt);
        presentViewModel(state, engine, localPawn ? std::optional<math::Vec3>(view.eye) : std::nullopt,
            playerSeconds);

        if (state.level)
        {
            presentSceneCharacters(engine.renderer, state.level->scene, state.characters, frameInfo.frameSeconds,
                view.eye, view.direction);
        }

        drawHud(engine.renderer, state.hud);

        if (presentFlyCamera(state, engine, frameInfo, view))
        {
            return;
        }

        engine.renderer.setCamera(firstPersonCamera(view.eye, view.direction,
            state.pawnCharacters ? viewModelZoom(state.pawnCharacters->viewModel) : 0.0f));

        if (localPawn)
        {
            drawCrosshair(engine.renderer, state.visuals, view.eye, view.direction);

            if (state.room && !state.room->prompt.empty())
            {
                drawInteractPrompt(engine.renderer, state.visuals, view.eye, view.direction);
            }
        }

        const bool isHurt = status.isDead || status.health < state.settings.pawns.maxHealth;

        if (status.isJoined && (!state.room || isHurt))
        {
            drawHealth(engine.renderer, state.visuals, view.eye, view.direction, status,
                state.settings.pawns.maxHealth);
        }
    }
} //namespace game
