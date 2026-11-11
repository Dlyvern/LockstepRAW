#include "Simulation/LevelFlow.hpp"

#include "GameState.hpp"
#include "Level/MorningRoutine.hpp"
#include "Player/LocalPlayer.hpp"
#include "Simulation/HospitalSimulation.hpp"
#include "Simulation/HostSimulation.hpp"
#include "Simulation/NpcSimulation.hpp"
#include "Simulation/RoomSimulation.hpp"
#include "Simulation/StationSimulation.hpp"

#include <Lockstep/Assets/FileSystem.hpp>
#include <Lockstep/Assets/SceneCollision.hpp>
#include <Lockstep/Net/Replication.hpp>
#include <Lockstep/Net/Session.hpp>
#include <Lockstep/Physics/PhysicsWorld.hpp>
#include <Lockstep/Render/Renderer.hpp>
#include <Lockstep/World/Entities.hpp>
#include <Lockstep/World/TimeDomains.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <utility>
#include <vector>

namespace game
{
    namespace
    {
        namespace assets = lockstep::assets;
        namespace net = lockstep::net;
        namespace world = lockstep::world;

        using lockstep::app::EngineContext;
        using lockstep::app::GameState;

        constexpr std::string_view k_nextLevelFlag{"--next-level"};
        constexpr std::byte k_levelTag{0x4C};
        constexpr std::size_t k_levelHeaderBytes{2};
        constexpr std::size_t k_maxLevelNameBytes{255};
        constexpr float k_switchHoldSeconds{1.0f};
        constexpr float k_enterHoldSeconds{0.4f};
        constexpr float k_enterFadeSeconds{1.6f};
        constexpr float k_autoplayStepSeconds{4.0f};
        constexpr std::array<world::EntityOwner, k_playerCount> k_pawnOwners{world::EntityOwner::Player1,
            world::EntityOwner::Player2};
        constexpr std::array<std::string_view, k_playerCount> k_playerStartNames{"PlayerStart_1", "PlayerStart_2"};
        constexpr math::Vec3 k_nodeForward{0.0f, 0.0f, -1.0f};
        constexpr uint64_t k_bytesPerMebibyte{1024 * 1024};

        lockstep::Result<std::string> parseNextLevel(std::span<const char* const> arguments)
        {
            for (std::size_t index = 0; index < arguments.size(); ++index)
            {
                if (arguments[index] == nullptr || std::string_view(arguments[index]) != k_nextLevelFlag)
                {
                    continue;
                }

                if (index + 1 >= arguments.size() || arguments[index + 1] == nullptr)
                {
                    return lockstep::Error{lockstep::ErrorCode::InvalidArgument,
                        "--next-level needs a level's virtual path, e.g. levels/station/scene"};
                }

                return std::string(arguments[index + 1]);
            }

            return std::string(k_defaultNextLevel);
        }

#if LOCKSTEP_DEV_TOOLS
        constexpr std::string_view k_autoplayFlag{"--dev-autoplay"};

        bool hasFlag(std::span<const char* const> arguments, std::string_view flag)
        {
            return std::any_of(arguments.begin(), arguments.end(), [flag](const char* argument)
            {
                return argument != nullptr && std::string_view(argument) == flag;
            });
        }
#endif

        std::string launchName(const SceneLaunch& launch)
        {
            return launch.level.empty() ? launch.path.string() : launch.level;
        }

        std::vector<std::byte> encodeLevelMessage(std::string_view level)
        {
            std::vector<std::byte> bytes{k_levelTag, static_cast<std::byte>(level.size())};

            for (const char character : level)
            {
                bytes.push_back(static_cast<std::byte>(character));
            }

            return bytes;
        }

        std::optional<std::string> decodeLevelMessage(std::span<const std::byte> bytes)
        {
            if (bytes.size() < k_levelHeaderBytes || bytes[0] != k_levelTag
                || bytes.size() != k_levelHeaderBytes + static_cast<std::size_t>(bytes[1]))
            {
                return std::nullopt;
            }

            std::string level;

            for (const std::byte byte : bytes.subspan(k_levelHeaderBytes))
            {
                level.push_back(static_cast<char>(byte));
            }

            return level;
        }

        bool isClientConnected(const EngineContext& engine)
        {
            return engine.session.state() == net::ConnectionState::Connected;
        }

        void sendLevel(GameState& state, EngineContext& engine)
        {
            const std::vector<std::byte> message = encodeLevelMessage(state.levelFlow.current);

            if (auto sent = engine.session.send(message, net::Delivery::Reliable); !sent)
            {
                std::clog << "Game: level switch not sent: " << sent.error().message << '\n';
                return;
            }

            state.levelFlow.isClientInformed = true;
            std::clog << "Game: told the client to load '" << state.levelFlow.current << "'\n";

            if (state.room)
            {
                state.room->sentState.reset();
            }

            if (state.station)
            {
                state.station->sentState.reset();
            }
        }

        lockstep::Status setUpLevel(GameState& state, EngineContext& engine, std::string_view name, bool isSwitch)
        {
            const std::string shortName = levelShortName(name);
            const assets::Scene& scene = state.level->scene;
            state.levelFlow.current = std::string(name);
            state.weaponZones = findWeaponZones(scene);
            state.characters = createSceneCharacters(scene, engine.arguments);
            const LevelSpawns spawns = findLevelSpawns(*state.level, engine.physics);
            state.settings.spawnPoints = spawns.pawns;
            state.settings.enemy.spawnPoint = spawns.enemy;
            state.dialogue = loadDialogue(engine.files, shortName);
            state.npcs = findNpcPlaceholders(scene, shortName);
            state.glass = findGlassPanes(*state.level, engine.physics);
            clearGlassDebris(state.glassShatter);
            state.hud.localPlayer = engine.session.role() == net::Role::Client ? k_clientPawn : k_hostPawn;

            if (engine.session.role() != net::Role::Client)
            {
                clearLevelNpcs(state, engine);
                state.simulation.checkpoint.reset();
            }

            if (startHospital(state, engine))
            {
                return {};
            }

            if (startStation(state))
            {
                return {};
            }

            if (isSwitch && !assets::findNodeIndex(scene, "Interact"))
            {
                return {};
            }

            return startRoom(state, engine, spawns);
        }

        void despawnToys(GameState& state, EngineContext& engine)
        {
            if (!state.room)
            {
                return;
            }

            for (Toy& toy : state.room->toys)
            {
                if (!toy.body.isNull())
                {
                    engine.physics.destroyBody(toy.body);
                }

                if (!toy.entity.isNull())
                {
                    engine.entities.destroy(toy.entity);
                }

                toy = Toy{};
            }
        }

        void logGpuMemory(const EngineContext& engine, std::string_view moment)
        {
            const lockstep::render::GpuMemoryStats memory = engine.renderer.memoryStats();
            std::clog << "Game: GPU memory " << moment << ": " << memory.deviceBytesUsed / k_bytesPerMebibyte
                << " MiB used (" << (memory.isBudgetFromDriver ? "driver" : "estimated") << "), "
                << memory.deviceBytesAllocated / k_bytesPerMebibyte << " MiB in " << memory.allocationCount
                << " allocations; " << memory.meshes << " meshes, " << memory.materials << " materials, "
                << memory.textures << " textures, " << memory.pendingReleases << " pending releases\n";
        }

        void releaseLevel(GameState& state, EngineContext& engine)
        {
            clearGore(state.gore, engine.physics);
            clearDroppedWeapons(state.npcLooks.weapons, engine.physics);

            if (state.level)
            {
                for (const lockstep::physics::BodyId blocker : state.level->blockers)
                {
                    engine.physics.destroyBody(blocker);
                }

                assets::releaseScene(engine.renderer, engine.physics, state.level->scene, state.level->collision);

                if (auto cleared = engine.renderer.setIrradianceVolume(nullptr); !cleared)
                {
                    std::clog << "Game: irradiance volume not cleared: " << cleared.error().message << '\n';
                }
            }

            state.room.reset();
            state.station.reset();
            state.level.reset();
            state.glass = BreakableGlass{};
            clearGlassDebris(state.glassShatter);

            if (auto released = engine.renderer.releaseDestroyed(); !released)
            {
                std::clog << "Game: old level's GPU memory not released early: " << released.error().message << '\n';
            }

            logGpuMemory(engine, "with the old level released");
        }

        lockstep::Status replaceLevel(GameState& state, EngineContext& engine, SceneLevel level, std::string_view name)
        {
            logGpuMemory(engine, "with the new level loaded");
            state.level = std::move(level);

            return setUpLevel(state, engine, name, true);
        }

        void faceLevelStart(GameState& state, std::size_t player)
        {
            if (!state.level)
            {
                return;
            }

            if (const std::optional<math::Transform> start = assets::findNode(state.level->scene,
                    k_playerStartNames[player]))
            {
                const math::Vec3 forward = start->rotation * k_nodeForward;
                state.local.controls.yaw = std::atan2(-forward.x, -forward.z);
                state.local.controls.pitch = 0.0f;
            }
        }

        void beginEntering(GameState& state, std::size_t localPlayer)
        {
            state.levelFlow.isEntering = true;
            state.levelFlow.enteredSeconds = 0.0f;
            state.hud.lines.clear();
            state.hud.caption.clear();
            state.hud.fade = 1.0f;
            setHudPrompt(state.hud, "");
            setHudObjective(state.hud, "");
            state.subtitles.queue.clear();
            faceLevelStart(state, localPlayer);
        }

        bool isLevelMounted(EngineContext& engine, const std::string& name)
        {
            if (engine.files.exists(name))
            {
                return true;
            }

            std::clog << "Game: level '" << name << "' is not in the mounted content; mount its pak with "
                "--content (e.g. --content Content/cooked/station.pak)\n";

            return false;
        }

        std::optional<SceneLevel> loadNamedLevel(EngineContext& engine, const std::string& name)
        {
            auto loaded = loadSceneLevel(SceneLaunch{.level = name, .path = {}, .scale = 1.0f}, engine.renderer, engine.physics, engine.files);

            if (!loaded)
            {
                std::clog << "Game: level '" << name << "' not loaded: " << loaded.error().message << '\n';
                return std::nullopt;
            }

            return std::move(loaded).value();
        }

        void respawnPawnsAtStart(GameState& state, EngineContext& engine, const std::array<bool, k_playerCount>& wasAlive)
        {
            for (std::size_t index = 0; index < k_playerCount; ++index)
            {
                Pawn& pawn = state.simulation.pawns[index];
                pawn.isArmed = state.settings.startArmed;
                pawn.isLowered = false;

                if (!wasAlive[index])
                {
                    continue;
                }

                if (auto spawned = spawnPawn(pawn, engine.entities, engine.replication, engine.physics,
                        k_pawnOwners[index], state.settings.spawnPoints[index], state.settings.pawns); !spawned)
                {
                    std::clog << "Game: " << (index == 0 ? "Player1" : "Player2") << " not spawned in the new level: "
                        << spawned.error().message << '\n';
                }
            }
        }

        void switchLevelAsHost(GameState& state, EngineContext& engine)
        {
            const std::string next = std::exchange(state.levelFlow.next, std::string{});
            std::clog << "Game: switching to level '" << next << "'\n";
            logGpuMemory(engine, "before the switch");

            if (!isLevelMounted(engine, next))
            {
                return;
            }

            despawnToys(state, engine);
            std::array<bool, k_playerCount> wasAlive{};

            for (std::size_t index = 0; index < k_playerCount; ++index)
            {
                Pawn& pawn = state.simulation.pawns[index];
                wasAlive[index] = !pawn.entity.isNull();

                if (wasAlive[index])
                {
                    despawnPawn(pawn, engine.entities, engine.physics);
                }
            }

            releaseLevel(state, engine);
            std::optional<SceneLevel> loaded = loadNamedLevel(engine, next);

            if (!loaded)
            {
                return;
            }

            if (auto replaced = replaceLevel(state, engine, std::move(*loaded), next); !replaced)
            {
                std::clog << "Game: level '" << next << "' set up with an error: " << replaced.error().message << '\n';
            }

            state.levelFlow.isClientInformed = false;

            if (isClientConnected(engine))
            {
                sendLevel(state, engine);
            }

            respawnPawnsAtStart(state, engine, wasAlive);
            state.simulation.lagHistory = LagHistory{};
            state.simulation.sentStatuses.reset();
            beginEntering(state, k_hostPawn);
        }

        bool isDriveOver(const GameState& state)
        {
            return state.room && state.room->state.phase == MorningPhase::MorningComplete
                && state.room->driveSeconds >= k_driveFadeSeconds + k_switchHoldSeconds;
        }

        void autoplay(GameState& state, EngineContext& engine, float playerDelta)
        {
            if (engine.session.role() == net::Role::Host && !isClientConnected(engine))
            {
                return;
            }

            LevelFlow& flow = state.levelFlow;
            flow.autoplaySeconds += playerDelta;

            if (flow.autoplaySeconds < k_autoplayStepSeconds)
            {
                return;
            }

            flow.autoplaySeconds = 0.0f;

            if (state.room && state.room->carSpot && state.room->state.phase != MorningPhase::MorningComplete)
            {
                state.room->state.phase = MorningPhase::MorningComplete;
                std::clog << "Game: dev autoplay: morning complete\n";
                return;
            }

            advanceStationForDev(state, engine);
        }
    } //namespace

    lockstep::Status startLevel(GameState& state, EngineContext& engine, const std::optional<SceneLaunch>& launch)
    {
        auto next = parseNextLevel(engine.arguments);

        if (!next)
        {
            return next.error();
        }

        state.levelFlow.next = std::move(next).value();

#if LOCKSTEP_DEV_TOOLS
        state.levelFlow.isAutoplay = hasFlag(engine.arguments, k_autoplayFlag);
#endif

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
        state.levelFlow.launched = launchName(*launch);

        if (state.levelFlow.next == state.levelFlow.launched)
        {
            state.levelFlow.next.clear();
        }

        if (engine.session.role() != net::Role::Client && !state.levelFlow.next.empty()
            && !engine.files.exists(state.levelFlow.next))
        {
            std::clog << "Game: the next level '" << state.levelFlow.next << "' is not in the mounted content; the "
                "morning will end without it (mount it, e.g. --content Content/cooked/station.pak)\n";
        }

        return setUpLevel(state, engine, state.levelFlow.launched, false);
    }

    void unloadLevel(GameState& state, EngineContext& engine)
    {
        despawnToys(state, engine);
        releaseLevel(state, engine);
        state.levelFlow = LevelFlow{};
    }

    void updateLevelFlowAsHost(GameState& state, EngineContext& engine)
    {
        LevelFlow& flow = state.levelFlow;

        if (!isClientConnected(engine))
        {
            flow.isClientInformed = false;
        }
        else if (!flow.isClientInformed && !flow.current.empty())
        {
            sendLevel(state, engine);
        }

        if (flow.isAutoplay)
        {
            autoplay(state, engine, engine.timeDomains.delta(world::TimeDomain::Player));
        }

        if (!flow.next.empty() && (isDriveOver(state) || flow.isSwitchRequested))
        {
            flow.isSwitchRequested = false;
            switchLevelAsHost(state, engine);
        }
    }

    void requestLevelSwitch(LevelFlow& flow, std::string_view level)
    {
        flow.next = std::string(level);
        flow.isSwitchRequested = true;
    }

    bool receiveLevelMessage(GameState& state, EngineContext& engine, std::span<const std::byte> message)
    {
        const std::optional<std::string> level = decodeLevelMessage(message);

        if (!level)
        {
            return false;
        }

        if (*level == state.levelFlow.current)
        {
            return true;
        }

        std::clog << "Game: the host switched to level '" << *level << "'\n";
        logGpuMemory(engine, "before the switch");

        if (!isLevelMounted(engine, *level))
        {
            return true;
        }

        clearPrediction(state.local.prediction, engine.entities, engine.physics);
        releaseLevel(state, engine);
        std::optional<SceneLevel> loaded = loadNamedLevel(engine, *level);

        if (!loaded)
        {
            return true;
        }

        if (auto replaced = replaceLevel(state, engine, std::move(*loaded), *level); !replaced)
        {
            std::clog << "Game: level '" << *level << "' set up with an error: " << replaced.error().message << '\n';
        }

        beginEntering(state, k_clientPawn);

        return true;
    }

    void presentLevel(GameState& state, EngineContext& engine)
    {
        const float playerDelta = engine.timeDomains.delta(world::TimeDomain::Player);
        const std::optional<math::Vec3> foot = engine.physics.footPosition(localController(state, engine));
        updateNpcBarks(state.npcs, state.dialogue, state.subtitles, foot, playerDelta);
        stepSubtitles(state.subtitles, playerDelta);
        setHudSubtitle(state.hud, currentSubtitle(state.subtitles));

        LevelFlow& flow = state.levelFlow;

        if (!flow.isEntering)
        {
            return;
        }

        flow.enteredSeconds += playerDelta;
        const float shown = std::clamp((flow.enteredSeconds - k_enterHoldSeconds) / k_enterFadeSeconds, 0.0f, 1.0f);
        state.hud.fade = std::max(state.hud.fade, 1.0f - shown);
        flow.isEntering = shown < 1.0f;

        if (!flow.isEntering)
        {
            logGpuMemory(engine, "after the switch");
        }
    }
} //namespace game
