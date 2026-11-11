#include "Presentation/Frame.hpp"

#include "Audio/CombatAudio.hpp"
#include "Effects/Effects.hpp"

#include "GameState.hpp"
#include "Gore/BodyStainEvents.hpp"
#include "Gore/CorpseHits.hpp"
#include "Lights/LightAnimation.hpp"
#include "Player/LocalPlayer.hpp"
#include "Player/PawnKick.hpp"
#include "Presentation/FirstPerson.hpp"
#include "Presentation/Hud.hpp"
#include "Presentation/LensEffects.hpp"
#include "Presentation/Mirrors.hpp"
#include "Presentation/WeaponPresentation.hpp"
#include "Simulation/HostSimulation.hpp"
#include "Weapons/WeaponFx.hpp"
#include "Weather/Weather.hpp"

#include <Lockstep/Net/Replication.hpp>
#include <Lockstep/Net/Session.hpp>
#include <Lockstep/Platform/Input.hpp>
#include <Lockstep/Platform/Window.hpp>
#include <Lockstep/Render/Renderer.hpp>
#include <Lockstep/World/Entities.hpp>
#include <Lockstep/World/TimeDomains.hpp>

#include <algorithm>
#include <cmath>
#include <optional>
#include <string>
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
        using lockstep::app::FrameInfo;
        using lockstep::app::GameState;

        constexpr math::Vec3 k_lobbyEye{0.0f, 6.0f, 18.0f};
        constexpr math::Vec3 k_lobbyLook{0.0f, -0.3f, -1.0f};
        constexpr math::Vec3 k_levelLobbyOffset{0.0f, 3.0f, 4.0f};

        struct View
        {
            math::Vec3 eye{0.0f, 0.0f, 0.0f};
            math::Vec3 direction{0.0f, 0.0f, -1.0f};
            math::Vec3 cameraDirection{0.0f, 0.0f, -1.0f};
            math::Vec3 up{0.0f, 1.0f, 0.0f};
        };

        void updateCursorCapture(platform::Window& window, const platform::Input& input, bool canCapture)
        {
            if (input.wasKeyPressed(platform::Key::F12))
            {
                window.setFullscreen(!window.isFullscreen());
            }

            if (canCapture && input.wasMouseButtonPressed(platform::MouseButton::Left))
            {
                window.setCursorCaptured(true);
            }
        }

        PawnCharacters* pawnCharacters(GameState& state)
        {
            return state.pawnCharacters ? &*state.pawnCharacters : nullptr;
        }

        FootstepPawn remoteFootsteps(const GameState& state, std::size_t index, const math::Transform& transform,
            lockstep::physics::ControllerId controller, world::EntityOwner owner)
        {
            const PawnLook look = remotePawnLook(state, owner, transform);

            return FootstepPawn{.index = index,
                .foot = pawnFootPosition(transform),
                .controller = controller,
                .isCrouched = look.isSliding || look.crouch > 0.5f,
                .isVaulting = look.isVaulting,
                .isKicking = look.isKicking};
        }

        void presentHost(GameState& state, EngineContext& engine, float alpha, float playerSeconds,
            float worldSeconds, std::optional<math::Transform>& localPawn, std::vector<FootstepPawn>& steps)
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

                steps.push_back(remoteFootsteps(state, ownerPawnIndex(entity->owner()), transform, pawn.controller,
                    entity->owner()));
                drawPawn(engine.renderer, state.visuals, pawnCharacters(state), entity->owner(), transform,
                    remotePawnLook(state, entity->owner(), transform), playerSeconds);
            }

            if (const world::Entity* enemy = engine.entities.find(state.simulation.enemy.entity))
            {
                drawEntity(engine.renderer, state.visuals, Archetype::Enemy, enemy->owner(),
                    enemy->interpolatedTransform(alpha));
            }

            for (const Npc& npc : state.simulation.npcs.npcs)
            {
                if (const world::Entity* entity = engine.entities.find(npc.entity))
                {
                    drawNpc(engine.renderer, engine.physics, state.npcLooks, pawnCharacters(state), state.visuals,
                        npc.faction == NpcFaction::Squad ? Archetype::Squad : Archetype::Merc, npc.entity,
                        entity->interpolatedTransform(alpha), npcView(npc), alpha, worldSeconds);
                }
            }
        }

        NpcView mirroredNpcView(const GameState& state, world::EntityId hostEntity)
        {
            for (const NpcView& view : state.local.npcViews.npcs)
            {
                if (view.entity == hostEntity)
                {
                    return view;
                }
            }

            return NpcView{.entity = hostEntity};
        }

        void presentClient(GameState& state, EngineContext& engine, float alpha, float playerSeconds,
            float worldSeconds, std::optional<math::Transform>& localPawn, std::vector<FootstepPawn>& steps)
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
                        steps.push_back(remoteFootsteps(state, ownerPawnIndex(entity->owner()), transform, {},
                            entity->owner()));
                        drawPawn(engine.renderer, state.visuals, pawnCharacters(state), entity->owner(), transform,
                            remotePawnLook(state, entity->owner(), transform), playerSeconds);
                    }

                    continue;
                }

                if (archetype == Archetype::Merc || archetype == Archetype::Squad)
                {
                    drawNpc(engine.renderer, engine.physics, state.npcLooks, pawnCharacters(state), state.visuals,
                        archetype, mirrored.entity, transform, mirroredNpcView(state, mirrored.hostEntity), alpha,
                        worldSeconds);
                    continue;
                }

                drawEntity(engine.renderer, state.visuals, archetype, entity->owner(), transform);
            }
        }

        void presentCombat(GameState& state, EngineContext& engine, const std::vector<FootstepPawn>& steps,
            const std::optional<math::Transform>& localPawn, float lookYaw, const math::Vec3& eye,
            const math::Vec3& direction, float seconds)
        {
            const std::size_t local = localPawnIndex(engine);
            const std::size_t partner = local == k_hostPawn ? k_clientPawn : k_hostPawn;
            const PlayerStatus& own = state.local.statuses[local];
            const PlayerStatus& other = state.local.statuses[partner];
            bool canRevive = false;

            if (localPawn && other.isDowned && !own.isDowned)
            {
                for (const FootstepPawn& step : steps)
                {
                    canRevive = canRevive || (!step.isLocal && step.index == partner
                        && math::length(step.foot - pawnFootPosition(*localPawn)) <= state.settings.downed.reviveRange);
                }
            }

            const std::string weaponLine = std::string(weaponName(shownWeapon(state).weapon)) + "   "
                + fireModeName(state.local.controls.trigger.mode);
            presentCombatHud(engine.renderer, state.visuals, state.local.combatHud,
                CombatHudFrame{.own = own,
                    .partner = other,
                    .isRestarting = state.local.isRestarting,
                    .canRevive = canRevive,
                    .isArmed = own.isArmed && localPawn.has_value(),
                    .magazine = state.local.controls.trigger.rounds,
                    .fireMode = weaponLine.c_str(),
                    .eye = eye,
                    .direction = direction,
                    .lookYaw = lookYaw,
                    .hudFade = state.hud.fade,
                    .seconds = seconds});
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
#if LOCKSTEP_DEV_TOOLS
            if (const std::optional<FlyCamera> fixed = devFixedCamera(engine.arguments))
            {
                state.flyCamera = *fixed;
                engine.renderer.setCamera(flyCameraView(state.flyCamera));

                return true;
            }
#endif

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

        void presentFootsteps(GameState& state, EngineContext& engine, std::vector<FootstepPawn>& steps,
            const std::optional<math::Transform>& localPawn, const HeadPose& head, float playerSeconds)
        {
            if (localPawn)
            {
                const PawnMotion& motion = localPawnMotion(state, engine);
                steps.push_back(FootstepPawn{.index = localPawnIndex(engine),
                    .foot = pawnFootPosition(*localPawn),
                    .controller = localController(state, engine),
                    .isLocal = true,
                    .footfalls = head.footfalls,
                    .landings = head.landings,
                    .landSpeed = head.landSpeed,
                    .speed = math::length(motion.planarVelocity),
                    .isCrouched = motion.stance != PawnStance::Stand,
                    .isVaulting = isVaulting(motion),
                    .isKicking = isKicking(motion)});
            }

            const PawnSettings& pawns = state.settings.pawns;
            const HeadMotionTuning& strides = state.headMotion.tuning;
            updateFootsteps(state.footsteps, engine.audio,
                FootstepFrame{.level = state.level ? &*state.level : nullptr,
                    .physics = &engine.physics,
                    .walkSpeed = pawns.walkSpeed,
                    .runSpeed = pawns.runSpeed,
                    .walkStride = strides.walkStride,
                    .runStride = strides.runStride,
                    .crouchStride = strides.crouchStride,
                    .walkStrideSpeed = strides.walkStrideSpeed,
                    .runStrideSpeed = strides.runStrideSpeed,
                    .worldScale = engine.timeDomains.scale(world::TimeDomain::World),
                    .seconds = playerSeconds},
                steps);
        }
        std::vector<NpcShotSource> npcShotSources(const GameState& state, const EngineContext& engine, bool isClient)
        {
            std::vector<NpcShotSource> sources;

            if (!isClient)
            {
                for (const Npc& npc : state.simulation.npcs.npcs)
                {
                    if (const world::Entity* entity = engine.entities.find(npc.entity))
                    {
                        sources.push_back(NpcShotSource{.key = npc.entity.index,
                            .position = entity->transform().position + math::Vec3{0.0f, 1.4f, 0.0f},
                            .shotCount = npc.shotCount});
                    }
                }

                return sources;
            }

            for (const net::MirroredEntity& mirrored : engine.replication.mirrored())
            {
                const world::Entity* entity = engine.entities.find(mirrored.entity);

                if (entity == nullptr)
                {
                    continue;
                }

                for (const NpcView& view : state.local.npcViews.npcs)
                {
                    if (view.entity == mirrored.hostEntity)
                    {
                        sources.push_back(NpcShotSource{.key = mirrored.hostEntity.index,
                            .position = entity->transform().position + math::Vec3{0.0f, 1.4f, 0.0f},
                            .shotCount = view.shotCount});
                    }
                }
            }

            return sources;
        }

        void presentCombatAudio(GameState& state, EngineContext& engine, const std::vector<FootstepPawn>& steps,
            const std::optional<math::Transform>& localPawn, const math::Vec3& eye, const math::Vec3& aim,
            float playerSeconds, bool isClient)
        {
            CombatAudioFrame frame{.scene = state.level ? &state.level->scene : nullptr,
                .physics = &engine.physics,
                .localIndex = localPawnIndex(engine),
                .localShots = state.local.command.shotCount,
                .localController = localPawn ? localController(state, engine) : lockstep::physics::ControllerId{},
                .eye = eye,
                .aim = aim,
                .command = &state.local.command,
                .level = state.level ? &*state.level : nullptr,
                .spread = &weaponStats(state.settings.weapons, state.local.command.weapon).spread,
                .penetration = &weaponStats(state.settings.weapons, state.local.command.weapon).penetration,
                .statuses = state.local.statuses,
                .listener = eye,
                .rain = rainSoundAmount(state.weather),
                .thunder = state.weather.tuning.sound ? std::span<const ThunderCue>(state.weather.lightning.due)
                    : std::span<const ThunderCue>{},
                .worldSeconds = playerSeconds * engine.timeDomains.scale(world::TimeDomain::World),
                .seconds = playerSeconds};

            for (const FootstepPawn& pawn : steps)
            {
                if (pawn.index < frame.positions.size())
                {
                    frame.positions[pawn.index] = pawn.foot + math::Vec3{0.0f, 1.5f, 0.0f};
                }
            }

            const std::vector<NpcShotSource> npcs = npcShotSources(state, engine, isClient);
            frame.npcs = npcs;
            updateCombatAudio(state.combatAudio, engine.audio, frame);
            presentLevelCurtains(state, engine, frame, playerSeconds * engine.timeDomains.scale(world::TimeDomain::World));
        }
    } //namespace

    void presentFrame(GameState& state, EngineContext& engine, const FrameInfo& frameInfo)
    {
        updateCursorCapture(engine.window, frameInfo.input, !isShellBlockingInput(state.shell));
        updateMaterialTimingDrill(state.materialDrill, engine.renderer);
        updateDevCapture(state.devCapture, engine.renderer,
            DevCaptureEvents{.isReloading = state.local.statuses[localPawnIndex(engine)].isReloading,
                .isSwitching = shownWeapon(state).isSwitching});
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

        if (!state.level)
        {
            submitArenaLights(engine.renderer, state.lighting);
        }

        std::optional<math::Transform> localPawn;
        const bool isClient = engine.session.role() == net::Role::Client;
        const float playerSeconds = frameInfo.frameSeconds * engine.timeDomains.scale(world::TimeDomain::Player);

        if (state.pawnCharacters)
        {
            beginPawnCharacters(*state.pawnCharacters);
        }

        std::vector<FootstepPawn> steps;
        const float worldSeconds = frameInfo.frameSeconds * engine.timeDomains.scale(world::TimeDomain::World);
        beginNpcLooks(state.npcLooks);
        prepareCharacterStains(state, engine, worldSeconds);

        if (isClient)
        {
            presentClient(state, engine, frameInfo.alpha, playerSeconds, worldSeconds, localPawn, steps);
        }
        else
        {
            presentHost(state, engine, frameInfo.alpha, playerSeconds, worldSeconds, localPawn, steps);
        }

        endNpcLooks(state.npcLooks, engine.physics, engine.renderer, frameInfo.alpha, worldSeconds);
        presentKicks(state, engine, worldSeconds);

        presentMirrors(engine.renderer, state.level ? &state.level->scene : nullptr);

        const FirstPersonLook look = presentedLook(state, engine, frameInfo);

        if (localPawn)
        {
            const PawnLook ownPawnLook = localPawnLook(state, engine, localPawn);
            const FirstPersonBodyView bodyView{.lookYaw = look.yaw,
                .lookPitch = look.pitch,
                .eye = eyePosition(*localPawn),
                .hasViewModel = ownPawnLook.isArmed && state.pawnCharacters
                    && firstPersonModel(*state.pawnCharacters, WeaponKind::Kriss) != nullptr};
            drawPawn(engine.renderer, state.visuals, pawnCharacters(state), localOwner(engine), *localPawn,
                ownPawnLook, playerSeconds, k_mirrorOnlyLayer, &bodyView);
        }

        HeadPose head{};
        View view = lobbyView(state);

        if (localPawn)
        {
            const float yaw = look.yaw;
            const float pitch = look.pitch;
            head = presentHead(state, engine, *localPawn, look, playerSeconds);
            view = View{.eye = head.eye,
                .direction = lookDirection(yaw, pitch),
                .cameraDirection = lookDirection(yaw + head.yawOffset, pitch + head.pitchOffset),
                .up = rolledUp(yaw + head.yawOffset, pitch + head.pitchOffset, head.roll)};
        }
        else
        {
            resetHeadMotion(state.headMotion);
        }

        presentFootsteps(state, engine, steps, localPawn, head, playerSeconds);
        const bool isFlying = state.flyCamera.isActive;
        presentWeather(state, engine, WeatherView{.eye = isFlying ? state.flyCamera.position : view.eye,
            .forward = isFlying ? lookDirection(state.flyCamera.yaw, state.flyCamera.pitch) : view.cameraDirection,
            .playerSeconds = playerSeconds}, worldSeconds, frameInfo.frameSeconds);
        presentViewModel(state, engine, localPawn ? std::optional<math::Vec3>(view.eye) : std::nullopt, head, look,
            playerSeconds);
        presentCombatAudio(state, engine, steps, localPawn, view.eye, view.direction, playerSeconds, isClient);
        presentGlassShatter(state, engine);
        presentWeaponFx(state, engine, WeaponFxFrame{.eye = view.eye,
            .aim = view.direction,
            .hasLocalPawn = localPawn.has_value(),
            .worldFov = firstPersonCamera(view.eye, view.cameraDirection,
                state.pawnCharacters ? viewModelZoom(state.pawnCharacters->viewModel) : 0.0f, view.up,
                localPawn ? head.fovOffset : 0.0f).verticalFov,
            .playerSeconds = playerSeconds,
            .worldSeconds = worldSeconds});
        presentDrips(state.drips, state, engine, state.flyCamera.isActive ? state.flyCamera.position : view.eye,
            worldSeconds);
        presentEffects(state, engine, state.flyCamera.isActive ? state.flyCamera.position : view.eye, worldSeconds,
            frameInfo.frameSeconds);
        presentToys(state, engine, frameInfo.alpha, localPawn ? std::optional<View>(view) : std::nullopt);
        const PawnLook ownLook = localPawnLook(state, engine);
        presentCorpseShots(state, engine, view.eye, localShotDirection(state, engine, state.local.command.shotCount),
            localPawn && ownLook.isArmed && !ownLook.isLowered && !isHoldingToy(state, engine));
        presentGore(state, engine, frameInfo.alpha, worldSeconds, view.eye);
        presentWeaponHandling(state, engine, localPawn, head, playerSeconds);

        if (state.level)
        {
            presentSceneCharacters(engine.renderer, state.level->scene, state.characters, frameInfo.frameSeconds,
                view.eye, view.direction);
            drawNpcPlaceholders(engine.renderer, state.npcs, pawnCharacters(state), playerSeconds);
        }

        drawHud(engine.renderer, state.hud);
        presentCombat(state, engine, steps, localPawn, look.yaw, view.eye, view.direction, frameInfo.frameSeconds);

        presentLevelLights(state.lightAnimation, engine, state.level ? &state.level->scene : nullptr,
            state.flyCamera.isActive ? state.flyCamera.position : view.eye, worldSeconds);
        presentLensEffects(engine.renderer.settings(), engine.timeDomains.scale(world::TimeDomain::World),
            heldBreathFocus(state), frameInfo.frameSeconds);

        if (presentFlyCamera(state, engine, frameInfo, view))
        {
            return;
        }

        lockstep::render::Camera camera = firstPersonCamera(view.eye, view.cameraDirection,
            state.pawnCharacters ? viewModelZoom(state.pawnCharacters->viewModel) : 0.0f, view.up,
            localPawn ? head.fovOffset : 0.0f);

        camera.shadowLayers = lockstep::render::k_defaultRenderLayer | k_mirrorOnlyLayer;

        if (localPawn)
        {
            camera.layers |= k_firstPersonBodyLayer;
        }

        engine.renderer.setCamera(camera);


        if (localPawn)
        {
            drawCrosshair(engine.renderer, state.visuals, view.eye, view.direction);

            const bool hasRoomPrompt = state.room && !state.room->prompt.empty();
            const bool hasStationPrompt = state.station && !state.station->prompt.empty();

            if (hasRoomPrompt || hasStationPrompt)
            {
                drawInteractPrompt(engine.renderer, state.visuals, view.eye, view.direction);
            }
        }
    }
} //namespace game
