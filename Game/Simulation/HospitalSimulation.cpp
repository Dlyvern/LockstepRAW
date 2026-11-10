#include "Simulation/HospitalSimulation.hpp"

#include "GameState.hpp"
#include "Npcs/NpcRoster.hpp"
#include "Player/LocalPlayer.hpp"
#include "Simulation/HostSimulation.hpp"
#include "Simulation/LevelFlow.hpp"
#include "Simulation/NpcSimulation.hpp"

#include <Lockstep/Assets/FileSystem.hpp>
#include <Lockstep/Net/Replication.hpp>
#include <Lockstep/Net/Session.hpp>
#include <Lockstep/Physics/PhysicsWorld.hpp>
#include <Lockstep/World/Entities.hpp>
#include <Lockstep/World/TimeDomains.hpp>

#include <algorithm>
#include <iostream>
#include <string_view>

namespace game
{
    namespace
    {
        namespace net = lockstep::net;
        namespace world = lockstep::world;

        using lockstep::app::EngineContext;
        using lockstep::app::GameState;

        constexpr std::string_view k_mercsFlag{"--dev-mercs"};
        constexpr std::string_view k_startBeatFlag{"--dev-start-beat"};
        constexpr float k_navMargin{15.0f};
        constexpr float k_navHeightMargin{1.0f};
        constexpr float k_beatReach{4.0f};
        constexpr float k_beatHeight{2.0f};
        constexpr float k_checkpointSide{1.0f};
        constexpr float k_liftReach{2.5f};
        constexpr float k_leaveFadeSeconds{2.0f};
        constexpr float k_leaveExitSeconds{2.5f};
        constexpr float k_leaveReturnSeconds{4.5f};
        constexpr std::size_t k_maxBeatBits{16};

        bool hasArgument(std::span<const char* const> arguments, std::string_view flag)
        {
            return std::any_of(arguments.begin(), arguments.end(), [flag](const char* argument)
            {
                return argument != nullptr && std::string_view(argument) == flag;
            });
        }

        void extend(math::Vec3& minimum, math::Vec3& maximum, const math::Vec3& point)
        {
            minimum = math::Vec3{std::min(minimum.x, point.x), std::min(minimum.y, point.y), std::min(minimum.z, point.z)};
            maximum = math::Vec3{std::max(maximum.x, point.x), std::max(maximum.y, point.y), std::max(maximum.z, point.z)};
        }

        void buildNavigation(GameState& state, EngineContext& engine)
        {
            const HospitalMission& mission = *state.hospital;
            math::Vec3 minimum = state.settings.spawnPoints[0];
            math::Vec3 maximum = minimum;

            for (const auto* markers : {&mission.beats, &mission.mercs, &mission.squad})
            {
                for (const HospitalMarker& marker : *markers)
                {
                    extend(minimum, maximum, marker.foot);
                }
            }

            for (const math::Vec3& spawn : state.settings.spawnPoints)
            {
                extend(minimum, maximum, spawn);
            }

            if (mission.lift)
            {
                extend(minimum, maximum, *mission.lift);
            }

            const lockstep::math::Aabb& bounds = state.level->scene.bounds;
            minimum = math::Vec3{minimum.x - k_navMargin, bounds.min.y - k_navHeightMargin, minimum.z - k_navMargin};
            maximum = math::Vec3{maximum.x + k_navMargin, bounds.max.y + k_navHeightMargin, maximum.z + k_navMargin};
            buildLevelNavigation(state, engine, minimum, maximum);
        }

        void spawnSquad(GameState& state, EngineContext& engine)
        {
            for (const HospitalMarker& marker : state.hospital->squad)
            {
                if (auto spawned = spawnNpc(state.simulation.npcs, engine.entities, engine.replication, engine.physics,
                        NpcSpawn{.faction = NpcFaction::Squad, .group = k_squadGroup, .name = marker.name,
                            .foot = marker.foot, .yaw = marker.yaw},
                        state.settings.npcs); !spawned)
                {
                    std::clog << "Game: squad officer '" << marker.name << "' not spawned: " << spawned.error().message
                              << '\n';
                }
            }
        }

        math::Vec3 checkpointFoot(const GameState& state, const math::Vec3& beat, float side)
        {
            const math::Vec3 wanted = beat + math::Vec3{side, 0.0f, 0.0f};

            if (state.simulation.nav)
            {
                if (const std::optional<uint32_t> node = nearestNavNode(*state.simulation.nav, wanted))
                {
                    return state.simulation.nav->nodes[*node].position;
                }
            }

            return beat;
        }

#if LOCKSTEP_DEV_TOOLS
        void startAtBeat(GameState& state, std::span<const char* const> arguments)
        {
            for (std::size_t index = 0; index + 1 < arguments.size(); ++index)
            {
                if (arguments[index] == nullptr || arguments[index + 1] == nullptr
                    || std::string_view(arguments[index]) != k_startBeatFlag)
                {
                    continue;
                }

                const std::string_view wanted = arguments[index + 1];

                for (const HospitalMarker& beat : state.hospital->beats)
                {
                    if (beat.name == wanted)
                    {
                        state.settings.spawnPoints = {checkpointFoot(state, beat.foot, -k_checkpointSide),
                            checkpointFoot(state, beat.foot, k_checkpointSide)};
                        std::clog << "Game: dev: players start at '" << beat.name << "'\n";
                        return;
                    }
                }

                std::clog << "Game: dev: no beat named '" << wanted << "'\n";
            }
        }
#endif

        void reachBeats(GameState& state, EngineContext& engine)
        {
            HospitalMission& mission = *state.hospital;

            for (std::size_t index = 0; index < mission.beats.size() && index < k_maxBeatBits; ++index)
            {
                const auto bit = static_cast<uint16_t>(1u << index);

                if ((mission.state.reachedBeats & bit) != 0)
                {
                    continue;
                }

                const HospitalMarker& beat = mission.beats[index];
                const bool isReached = std::any_of(state.simulation.pawns.begin(), state.simulation.pawns.end(),
                    [&engine, &beat](const Pawn& pawn)
                {
                    const std::optional<math::Vec3> foot = engine.physics.footPosition(pawn.controller);
                    const math::Vec3 offset = foot ? *foot - beat.foot : math::Vec3{1.0e6f, 0.0f, 0.0f};

                    return !pawn.isDowned && std::abs(offset.y) < k_beatHeight
                        && math::length(math::Vec3{offset.x, 0.0f, offset.z}) < k_beatReach;
                });

                if (!isReached)
                {
                    continue;
                }

                mission.state.reachedBeats = static_cast<uint16_t>(mission.state.reachedBeats | bit);
                mission.checkpointBeat = index;
                state.simulation.checkpoint = std::array<math::Vec3, k_playerCount>{
                    checkpointFoot(state, beat.foot, -k_checkpointSide), checkpointFoot(state, beat.foot, k_checkpointSide)};
                std::clog << "Game: hospital: reached '" << beat.name << "' (checkpoint, floor " << int{beat.floor}
                          << ")\n";

                if (mission.areMercsEnabled)
                {
                    spawnMercFloor(state, engine, beat.floor);
                }
            }
        }

        void updateLeaving(GameState& state, EngineContext& engine)
        {
            HospitalMission& mission = *state.hospital;

            if (!mission.state.isLeaving)
            {
                mission.leaveSeconds = 0.0f;
                return;
            }

            mission.leaveSeconds += engine.timeDomains.delta(world::TimeDomain::Player);

            if (mission.leaveSeconds >= k_leaveExitSeconds && !mission.isExitHandled)
            {
                mission.isExitHandled = true;

                if (engine.files.exists(std::string(k_hospitalNextLevel)))
                {
                    std::clog << "Game: hospital: the lift goes down to '" << k_hospitalNextLevel << "'\n";
                    requestLevelSwitch(state.levelFlow, k_hospitalNextLevel);
                    return;
                }

                std::clog << "Game: hospital: Part A done; '" << k_hospitalNextLevel
                          << "' is not built or not mounted, staying here\n";
                addHudLine(state.hud, "The lift goes nowhere yet (Part B is not built).");
            }

            if (mission.leaveSeconds >= k_leaveReturnSeconds)
            {
                mission.state.isLeaving = false;
                mission.isExitHandled = false;
            }
        }

        void presentHospital(GameState& state, EngineContext& engine)
        {
            HospitalMission& mission = *state.hospital;
            const std::string_view objective = hospitalObjectiveId(mission);
            setHudObjective(state.hud, objective.empty() ? std::string_view{} : lineText(state.dialogue, objective));

            if (engine.session.role() == net::Role::Client)
            {
                mission.leaveSeconds = mission.state.isLeaving
                    ? mission.leaveSeconds + engine.timeDomains.delta(world::TimeDomain::Player) : 0.0f;
            }

            state.hud.fade = mission.state.isLeaving ? std::min(mission.leaveSeconds / k_leaveFadeSeconds, 1.0f) : 0.0f;
            const std::optional<math::Vec3> foot = engine.physics.footPosition(localController(state, engine));
            const bool isNearLift = foot && mission.lift && !mission.state.isLeaving
                && math::length(*mission.lift - *foot) < k_liftReach + 1.0f;
            setHudPrompt(state.hud, isNearLift ? "F  Take the service lift" : "");
        }

        void publishHospital(GameState& state, EngineContext& engine)
        {
            HospitalMission& mission = *state.hospital;

            if (engine.session.state() != net::ConnectionState::Connected)
            {
                mission.sentState.reset();
                return;
            }

            if (mission.sentState == mission.state)
            {
                return;
            }

            if (auto sent = engine.session.send(encodeHospitalState(mission.state), net::Delivery::Reliable); !sent)
            {
                std::clog << "Game: hospital state not sent: " << sent.error().message << '\n';
                return;
            }

            mission.sentState = mission.state;
        }
    } //namespace

    bool startHospital(GameState& state, EngineContext& engine)
    {
        state.hospital = findHospitalMission(state.level->scene);

        if (!state.hospital)
        {
            return false;
        }

        state.settings.hasEnemy = false;
        state.settings.startArmed = true;

        if (engine.session.role() == net::Role::Client)
        {
            return true;
        }

        state.simulation.isCombatActive = true;
        buildNavigation(state, engine);
        spawnSquad(state, engine);

#if LOCKSTEP_DEV_TOOLS
        startAtBeat(state, engine.arguments);

        if (hasArgument(engine.arguments, k_mercsFlag))
        {
            setMercsEnabled(state, engine, true);
        }
#else
        static_cast<void>(hasArgument);
#endif

        return true;
    }

    void runHospitalHostTick(GameState& state, EngineContext& engine)
    {
        if (!state.hospital)
        {
            return;
        }

        reachBeats(state, engine);
        updateLeaving(state, engine);
        presentHospital(state, engine);
        publishHospital(state, engine);
    }

    void runHospitalClientTick(GameState& state, EngineContext& engine)
    {
        if (state.hospital)
        {
            presentHospital(state, engine);
        }
    }

    bool receiveHospitalMessage(GameState& state, std::span<const std::byte> message)
    {
        const std::optional<HospitalState> received = decodeHospitalState(message);

        if (!received)
        {
            return false;
        }

        if (state.hospital)
        {
            state.hospital->state = *received;
        }

        return true;
    }

    void useHospital(GameState& state, EngineContext& engine, std::size_t pawnIndex)
    {
        HospitalMission& mission = *state.hospital;
        const Pawn& pawn = state.simulation.pawns[pawnIndex];
        const std::optional<math::Vec3> foot = engine.physics.footPosition(pawn.controller);

        if (!mission.lift || !foot || pawn.isDowned || mission.state.isLeaving
            || math::length(*mission.lift - *foot) > k_liftReach + 1.0f)
        {
            return;
        }

        mission.state.isLeaving = true;
        mission.leaveSeconds = 0.0f;
        std::clog << "Game: hospital: " << playerName(pawnIndex) << " takes the service lift\n";
    }

    void spawnMercFloor(GameState& state, EngineContext& engine, std::size_t floor)
    {
        if (!state.hospital || floor >= k_mercFloors || state.hospital->spawnedFloors[floor])
        {
            return;
        }

        state.hospital->spawnedFloors[floor] = true;
        std::size_t spawned = 0;

        for (const HospitalMarker& marker : state.hospital->mercs)
        {
            if (marker.floor != floor)
            {
                continue;
            }

            if (auto created = spawnNpc(state.simulation.npcs, engine.entities, engine.replication, engine.physics,
                    NpcSpawn{.faction = NpcFaction::Merc, .group = static_cast<uint8_t>(floor), .name = marker.name,
                        .foot = marker.foot, .yaw = marker.yaw},
                    state.settings.npcs); !created)
            {
                std::clog << "Game: merc '" << marker.name << "' not spawned: " << created.error().message << '\n';
                continue;
            }

            ++spawned;
        }

        std::clog << "Game: hospital: " << spawned << " mercs spawned on floor " << floor << '\n';
    }

    void setMercsEnabled(GameState& state, EngineContext& engine, bool enabled)
    {
        if (!state.hospital)
        {
            return;
        }

        HospitalMission& mission = *state.hospital;
        mission.areMercsEnabled = enabled;
        std::clog << "Game: hospital: mercs " << (enabled ? "enabled" : "disabled") << '\n';

        if (!enabled)
        {
            return;
        }

        spawnMercFloor(state, engine, 0);

        for (std::size_t index = 0; index < mission.beats.size() && index < k_maxBeatBits; ++index)
        {
            if ((mission.state.reachedBeats & (1u << index)) != 0)
            {
                spawnMercFloor(state, engine, mission.beats[index].floor);
            }
        }
    }

    void removeMercs(GameState& state, EngineContext& engine)
    {
        NpcRoster& roster = state.simulation.npcs;

        for (Npc& npc : roster.npcs)
        {
            if (npc.faction == NpcFaction::Merc)
            {
                engine.entities.destroy(npc.entity);
                engine.physics.destroyController(npc.controller);
            }
        }

        std::erase_if(roster.npcs, [](const Npc& npc) { return npc.faction == NpcFaction::Merc; });

        for (Npc& npc : roster.npcs)
        {
            npc.target = {};
            npc.canSeeTarget = false;
        }

        for (std::size_t group = 0; group < k_mercFloors; ++group)
        {
            roster.groups[group] = NpcGroup{};
        }

        if (state.hospital)
        {
            state.hospital->spawnedFloors = {};
        }

        std::clog << "Game: hospital: mercs removed\n";
    }
} //namespace game
