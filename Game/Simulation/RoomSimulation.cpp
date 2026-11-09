#include "Simulation/RoomSimulation.hpp"

#include "GameState.hpp"
#include "Simulation/HostSimulation.hpp"

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
            applyRoomState(room, *state.level, engine.physics, engine.timeDomains.delta(world::TimeDomain::World));
            showRoomChanges(room, localPlayer);

            if (localPawn == nullptr)
            {
                room.lookTarget.reset();
                room.prompt = "";
                return;
            }

            updateLookPrompt(room, *state.level, engine.physics, eyePosition(localPawn->transform()),
                lookDirection(state.local.command.yaw, state.local.command.pitch), localController, localPlayer);
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

        if (state.room)
        {
            state.room->echoPath = launch.value().savePath;
            state.room->shownState = state.room->state;
            state.settings.hasEnemy = false;
        }

        return {};
    }

    void interactWithRoom(GameState& state, EngineContext& engine, std::size_t pawnIndex)
    {
        Pawn& pawn = state.simulation.pawns[pawnIndex];

        while (pawn.hasCommand && pawn.processedInteracts != pawn.command.interactCount)
        {
            ++pawn.processedInteracts;
            const world::Entity* entity = engine.entities.find(pawn.entity);

            if (!state.room || !state.level || entity == nullptr)
            {
                continue;
            }

            const std::size_t partner = pawnIndex == k_hostPawn ? k_clientPawn : k_hostPawn;
            interactAsHost(*state.room, *state.level, engine.physics, pawnIndex, eyePosition(entity->transform()),
                lookDirection(pawn.command.yaw, pawn.command.pitch), pawn.controller,
                state.simulation.pawns[partner].isJoined);
        }
    }

    void runRoomHostTick(GameState& state, EngineContext& engine)
    {
        if (!state.room || !state.level)
        {
            return;
        }

        ApartmentRoom& room = *state.room;
        updateRoomAsHost(room, engine.timeDomains.delta(world::TimeDomain::World),
            engine.timeDomains.delta(world::TimeDomain::Player));
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
