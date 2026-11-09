#include "Simulation/RoomSimulation.hpp"

#include "GameState.hpp"
#include "Level/MorningRoutine.hpp"
#include "Simulation/HostSimulation.hpp"
#include "Simulation/StationSimulation.hpp"

#include <Lockstep/Net/Replication.hpp>
#include <Lockstep/Net/Session.hpp>
#include <Lockstep/Physics/PhysicsWorld.hpp>
#include <Lockstep/World/Entities.hpp>
#include <Lockstep/World/TimeDomains.hpp>

#include <iostream>

namespace game
{
    namespace
    {
        namespace net = lockstep::net;
        namespace world = lockstep::world;

        using lockstep::app::EngineContext;
        using lockstep::app::GameState;

        constexpr float k_throwSpeed{9.0f};
        constexpr float k_throwLift{1.5f};
        constexpr float k_dropSpeed{1.0f};
        constexpr std::string_view k_driveCaptionId{"morning.caption.drive"};

        std::optional<ToyPlayer> toyPlayer(const GameState& state, const EngineContext& engine, std::size_t pawnIndex)
        {
            const Pawn& pawn = state.simulation.pawns[pawnIndex];
            const world::Entity* entity = engine.entities.find(pawn.entity);
            const std::optional<math::Vec3> foot = engine.physics.footPosition(pawn.controller);

            if (entity == nullptr || !foot)
            {
                return std::nullopt;
            }

            const float delta = engine.timeDomains.delta(world::TimeDomain::Player);
            const math::Vec3 moved = entity->transform().position - entity->previousTransform().position;

            return ToyPlayer{.foot = *foot,
                .velocity = delta > 0.0f ? moved * (1.0f / delta) : math::Vec3{0.0f, 0.0f, 0.0f},
                .eye = eyePosition(entity->transform()),
                .direction = lookDirection(pawn.command.yaw, pawn.command.pitch),
                .controller = pawn.controller};
        }

        void releaseHeldToy(GameState& state, EngineContext& engine, std::size_t pawnIndex, const math::Vec3& velocity,
            RoomAction action)
        {
            ApartmentRoom& room = *state.room;
            const uint8_t held = room.state.heldToy[pawnIndex];

            if (held >= room.toys.size())
            {
                return;
            }

            if (auto released = releaseToy(room.toys[held], engine.entities, engine.physics, velocity); !released)
            {
                std::clog << "Game: toy not released: " << released.error().message << '\n';
                return;
            }

            room.state.heldToy[pawnIndex] = k_nobody;
            recordEchoEvent(room, pawnIndex, RoomObject::Toy, action);
            std::clog << "Game: room: " << playerName(pawnIndex) << " " << toString(action) << ": "
                << room.toys[held].name << '\n';
        }

        bool useToy(GameState& state, EngineContext& engine, std::size_t pawnIndex, const ToyPlayer& player)
        {
            ApartmentRoom& room = *state.room;

            if (room.state.heldToy[pawnIndex] != k_nobody)
            {
                releaseHeldToy(state, engine, pawnIndex, player.direction * k_dropSpeed, RoomAction::DropToy);
                return true;
            }

            const std::optional<std::size_t> toy = traceToy(room.toys, engine.entities, engine.physics,
                room.state.heldToy, player.eye, player.direction, k_interactReach, player.controller);

            if (!toy)
            {
                return false;
            }

            pickUpToy(room.toys[*toy], engine.physics);
            room.state.heldToy[pawnIndex] = static_cast<uint8_t>(*toy);
            recordEchoEvent(room, pawnIndex, RoomObject::Toy, RoomAction::PickUpToy);
            std::clog << "Game: room: " << playerName(pawnIndex) << " picked up " << room.toys[*toy].name << '\n';

            return true;
        }

        void updateToys(GameState& state, EngineContext& engine)
        {
            std::array<std::optional<ToyPlayer>, k_playerCount> players{};
            std::array<std::optional<math::Vec3>, k_playerCount> feet{};

            for (std::size_t index = 0; index < k_playerCount; ++index)
            {
                players[index] = toyPlayer(state, engine, index);
                feet[index] = players[index] ? std::optional<math::Vec3>(players[index]->foot) : std::nullopt;
            }

            ApartmentRoom& room = *state.room;

            for (std::size_t index = 0; index < k_playerCount; ++index)
            {
                if (!players[index] && room.state.heldToy[index] != k_nobody)
                {
                    releaseHeldToy(state, engine, index, math::Vec3{0.0f, 0.0f, 0.0f}, RoomAction::DropToy);
                }
            }

            updateToysAsHost(room.toys, room.state.heldToy, players, engine.entities, engine.physics,
                engine.timeDomains.delta(world::TimeDomain::World));
            updateMorningAsHost(room, feet);
        }

        bool isClientConnected(const EngineContext& engine)
        {
            return engine.session.state() == net::ConnectionState::Connected;
        }

        EchoFrame sampleEchoFrame(const GameState& state, const EngineContext& engine)
        {
            EchoFrame frame{};

            for (std::size_t index = 0; index < k_playerCount; ++index)
            {
                const Pawn& pawn = state.simulation.pawns[index];

                if (const std::optional<math::Vec3> foot = engine.physics.footPosition(pawn.controller))
                {
                    frame.pawns[index] = EchoPawnSample{.isPresent = true, .foot = *foot, .yaw = pawn.command.yaw,
                        .pitch = pawn.command.pitch};
                }
            }

            return frame;
        }

        void publishRoomState(ApartmentRoom& room, EngineContext& engine)
        {
            if (!isClientConnected(engine))
            {
                room.sentState.reset();
                return;
            }

            const auto encoded = encodeRoomState(room.state);

            if (room.sentState == encoded)
            {
                return;
            }

            if (auto sent = engine.session.send(encoded, net::Delivery::Reliable); !sent)
            {
                std::clog << "Game: room state not sent: " << sent.error().message << '\n';
                return;
            }

            room.sentState = encoded;
        }

        void presentRoom(GameState& state, EngineContext& engine, const world::Entity* localPawn,
            lockstep::physics::ControllerId localController, std::size_t localPlayer)
        {
            ApartmentRoom& room = *state.room;
            Hud& hud = state.hud;
            const float playerDelta = engine.timeDomains.delta(world::TimeDomain::Player);
            applyRoomState(room, *state.level, engine.physics, engine.timeDomains.delta(world::TimeDomain::World));
            stepHud(hud, playerDelta);

            for (const std::string_view id : takeRoomChanges(room, localPlayer))
            {
                addHudLine(hud, lineText(state.dialogue, id));
            }

            stepDrive(room, playerDelta);
            hud.fade = driveFade(room);
            hud.caption = room.state.phase == MorningPhase::MorningComplete ? lineText(state.dialogue, k_driveCaptionId)
                : std::string_view{};

            if (localPawn == nullptr)
            {
                room.lookTarget.reset();
                room.lookToy.reset();
                room.prompt.clear();
                setHudPrompt(hud, "");
                return;
            }

            const bool isAtCar = isNearCar(room, pawnFootPosition(localPawn->transform()));
            const std::string_view objective = objectiveId(room.state, localPlayer, isAtCar);
            setHudObjective(hud, objective.empty() ? objective : lineText(state.dialogue, objective));
            updateLookPrompt(room, *state.level, engine.physics, engine.entities, eyePosition(localPawn->transform()),
                lookDirection(state.local.command.yaw, state.local.command.pitch), localController, localPlayer);
            setHudPrompt(hud, room.state.phase == MorningPhase::MorningComplete ? "" : room.prompt);
        }
    } //namespace

    lockstep::Status startRoom(GameState& state, EngineContext& engine, const LevelSpawns& spawns)
    {
        auto launch = parseEchoLaunch(engine.arguments);

        if (!launch)
        {
            return launch.error();
        }

        if (launch.value().playPath)
        {
            if (auto played = playEcho(state, *launch.value().playPath); !played)
            {
                return played;
            }
        }

        if (!state.level)
        {
            return {};
        }

        state.room = createApartmentRoom(*state.level, spawns);

        if (!state.room)
        {
            return {};
        }

        ApartmentRoom& room = *state.room;
        room.echoPath = launch.value().savePath;
        room.shownState = room.state;
        state.settings.hasEnemy = false;
        state.hud.localPlayer = engine.session.role() == net::Role::Client ? k_clientPawn : k_hostPawn;
        hideToyInstances(room.toys, *state.level, engine.physics);

        if (engine.session.role() == net::Role::Client)
        {
            room.isRecording = false;
            return {};
        }

        std::clog << "Game: room: the echo will be saved to '" << room.echoPath.string() << "'\n";

        return spawnToys(room.toys, engine.entities, engine.replication, engine.physics);
    }

    void interactWithRoom(GameState& state, EngineContext& engine, std::size_t pawnIndex)
    {
        Pawn& pawn = state.simulation.pawns[pawnIndex];

        while (pawn.hasCommand && pawn.processedInteracts != pawn.command.interactCount)
        {
            ++pawn.processedInteracts;

            if (state.station)
            {
                useStation(state, engine, pawnIndex);
                continue;
            }

            const world::Entity* entity = engine.entities.find(pawn.entity);

            if (!state.room || !state.level || entity == nullptr)
            {
                continue;
            }

            if (const std::optional<ToyPlayer> player = toyPlayer(state, engine, pawnIndex);
                player && useToy(state, engine, pawnIndex, *player))
            {
                continue;
            }

            const Pawn& partner = state.simulation.pawns[pawnIndex == k_hostPawn ? k_clientPawn : k_hostPawn];
            const std::optional<math::Vec3> partnerFoot = partner.isJoined
                ? engine.physics.footPosition(partner.controller) : std::nullopt;
            const std::optional<RoomAction> action = interactAsHost(*state.room, *state.level, engine.physics,
                pawnIndex, eyePosition(entity->transform()), lookDirection(pawn.command.yaw, pawn.command.pitch),
                pawn.controller, partnerFoot);

            if (action == RoomAction::TakeGear)
            {
                pawn.isArmed = true;
            }
        }
    }

    bool throwHeldToy(GameState& state, EngineContext& engine, std::size_t pawnIndex)
    {
        if (!state.room || state.room->state.heldToy[pawnIndex] == k_nobody)
        {
            return false;
        }

        const Pawn& pawn = state.simulation.pawns[pawnIndex];
        const math::Vec3 direction = lookDirection(pawn.command.yaw, pawn.command.pitch);
        releaseHeldToy(state, engine, pawnIndex, direction * k_throwSpeed + math::Vec3{0.0f, k_throwLift, 0.0f},
            RoomAction::ThrowToy);

        return true;
    }

    void runRoomHostTick(GameState& state, EngineContext& engine)
    {
        if (!state.room || !state.level)
        {
            return;
        }

        ApartmentRoom& room = *state.room;
        updateRoomAsHost(room, engine.timeDomains.delta(world::TimeDomain::World));
        updateToys(state, engine);
        recordEchoFrame(room, sampleEchoFrame(state, engine));
        publishRoomState(room, engine);

        const Pawn& hostPawn = state.simulation.pawns[k_hostPawn];
        presentRoom(state, engine, engine.entities.find(hostPawn.entity), hostPawn.controller, k_hostPawn);
    }

    void runRoomClientTick(GameState& state, EngineContext& engine)
    {
        if (!state.room || !state.level)
        {
            return;
        }

        bindMirroredToys(state.room->toys, engine.replication.mirrored());
        const Prediction& prediction = state.local.prediction;
        presentRoom(state, engine, engine.entities.find(prediction.entity), prediction.controller, k_clientPawn);
    }

    bool receiveRoomMessage(GameState& state, std::span<const std::byte> message)
    {
        const std::optional<RoomState> received = decodeRoomState(message);

        if (!received)
        {
            return false;
        }

        if (state.room)
        {
            state.room->state = *received;
        }

        return true;
    }

    void stepEcho(GameState& state)
    {
        if (state.echo)
        {
            stepEchoPlayback(*state.echo);
        }
    }

    lockstep::Status playEcho(GameState& state, const std::filesystem::path& path)
    {
        auto recording = loadEchoRecording(path);

        if (!recording)
        {
            return recording.error();
        }

        std::clog << "Game: echo playing '" << path.string() << "': " << recording.value().frames.size()
            << " ticks, " << recording.value().events.size() << " interactions\n";
        state.echo = EchoPlayback{.recording = std::move(recording).value(), .frame = 0, .nextEvent = 0};

        return {};
    }
} //namespace game
