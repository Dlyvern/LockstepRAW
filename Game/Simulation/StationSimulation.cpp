#include "Simulation/StationSimulation.hpp"

#include "GameState.hpp"
#include "Simulation/HostSimulation.hpp"

#include <Lockstep/Net/Session.hpp>
#include <Lockstep/Physics/PhysicsWorld.hpp>
#include <Lockstep/World/Entities.hpp>
#include <Lockstep/World/TimeDomains.hpp>

#include <array>
#include <iostream>
#include <optional>

namespace game
{
    namespace
    {
        namespace net = lockstep::net;
        namespace world = lockstep::world;

        using lockstep::app::EngineContext;
        using lockstep::app::GameState;

        constexpr std::string_view k_missionCaptionId{"station.caption.mission"};
        constexpr std::string_view k_rangeIntroPrefix{"station.range.intro."};
        constexpr std::string_view k_rangeIntroFallback{"station.range.intro.01"};
        constexpr std::string_view k_rangeDonePrefix{"station.range.done."};
        constexpr std::string_view k_rangeDoneFallback{"station.range.done.01"};

        void speak(GameState& state, StationVoice voice, std::size_t localPlayer)
        {
            Subtitles& subtitles = state.subtitles;
            DialogueBook& dialogue = state.dialogue;

            switch (voice)
            {
                case StationVoice::Briefing:
                    subtitles.queue.clear();
                    queueLines(subtitles, dialogue, k_briefingPrefix, k_briefingFallback);
                    break;
                case StationVoice::BriefingOver:
                    subtitles.queue.clear();
                    break;
                case StationVoice::GiveArmor:
                    queueLine(subtitles, dialogue, "station.armorer.give.armor");
                    break;
                case StationVoice::GiveWeapon:
                    queueLine(subtitles, dialogue, "station.armorer.give.weapon");
                    break;
                case StationVoice::GiveAmmo:
                    queueLine(subtitles, dialogue, "station.armorer.give.ammo");
                    break;
                case StationVoice::RangeIntro:
                    queueLines(subtitles, dialogue, k_rangeIntroPrefix, k_rangeIntroFallback);
                    break;
                case StationVoice::Qualified:
                {
                    const std::vector<const DialogueLine*> lines = linesWithPrefix(dialogue, k_rangeDonePrefix);
                    queueLine(subtitles, dialogue,
                        lines.empty() ? k_rangeDoneFallback : std::string_view(lines[localPlayer % lines.size()]->id));
                    break;
                }
            }
        }

        std::array<std::optional<math::Vec3>, k_playerCount> presentFeet(const GameState& state,
            const EngineContext& engine)
        {
            std::array<std::optional<math::Vec3>, k_playerCount> feet{};

            for (std::size_t index = 0; index < k_playerCount; ++index)
            {
                const Pawn& pawn = state.simulation.pawns[index];

                if (pawn.isJoined && !pawn.entity.isNull())
                {
                    feet[index] = engine.physics.footPosition(pawn.controller);
                }
            }

            return feet;
        }

        void publishStationState(StationMission& mission, EngineContext& engine)
        {
            if (engine.session.state() != net::ConnectionState::Connected)
            {
                mission.sentState.reset();
                return;
            }

            const auto encoded = encodeStationState(mission.state);

            if (mission.sentState == encoded)
            {
                return;
            }

            if (auto sent = engine.session.send(encoded, net::Delivery::Reliable); !sent)
            {
                std::clog << "Game: station state not sent: " << sent.error().message << '\n';
                return;
            }

            mission.sentState = encoded;
        }

        void presentStation(GameState& state, EngineContext& engine, const world::Entity* localPawn,
            lockstep::physics::ControllerId localController, std::size_t localPlayer)
        {
            StationMission& mission = *state.station;
            Hud& hud = state.hud;
            const float playerDelta = engine.timeDomains.delta(world::TimeDomain::Player);
            applyStationState(mission, *state.level, engine.physics, engine.timeDomains.delta(world::TimeDomain::World));
            const StationChanges changes = describeStationChanges(mission.shownState, mission.state, localPlayer,
                rangeTargetMask(mission));
            mission.shownState = mission.state;
            stepHud(hud, playerDelta);

            for (const std::string_view id : changes.lines)
            {
                addHudLine(hud, lineText(state.dialogue, id));
            }

            for (const StationVoice voice : changes.voices)
            {
                speak(state, voice, localPlayer);
            }

            stepMissionStart(mission, playerDelta);
            const bool isOver = mission.state.phase == StationPhase::MissionStart;
            hud.fade = missionStartFade(mission);
            hud.caption = isOver ? lineText(state.dialogue, k_missionCaptionId) : std::string_view{};

            if (localPawn == nullptr)
            {
                mission.lookTarget.reset();
                mission.prompt.clear();
                setHudPrompt(hud, "");
                return;
            }

            const math::Vec3 foot = pawnFootPosition(localPawn->transform());

            if (takeRangeIntroduction(mission, foot, localPlayer))
            {
                speak(state, StationVoice::RangeIntro, localPlayer);
            }

            setHudObjective(hud, stationObjective(mission, state.dialogue, localPlayer, isNearStationCar(mission, foot)));
            updateStationLook(mission, *state.level, engine.physics, eyePosition(localPawn->transform()),
                lookDirection(state.local.command.yaw, state.local.command.pitch), localController, localPlayer);
            setHudPrompt(hud, isOver ? "" : mission.prompt);
        }
    } //namespace

    bool startStation(GameState& state)
    {
        if (!state.level)
        {
            return false;
        }

        state.station = createStationMission(*state.level, state.dialogue);

        if (!state.station)
        {
            return false;
        }

        state.station->shownState = state.station->state;
        state.settings.hasEnemy = false;

        return true;
    }

    void useStation(GameState& state, EngineContext& engine, std::size_t pawnIndex)
    {
        Pawn& pawn = state.simulation.pawns[pawnIndex];
        const world::Entity* entity = engine.entities.find(pawn.entity);

        if (!state.station || !state.level || entity == nullptr)
        {
            return;
        }

        const std::optional<StationObject> used = useStationAsHost(*state.station, *state.level, engine.physics,
            pawnIndex, eyePosition(entity->transform()), lookDirection(pawn.command.yaw, pawn.command.pitch),
            pawn.controller, presentFeet(state, engine));

        if (used == StationObject::Weapon)
        {
            pawn.isArmed = true;
        }
    }

    void shootStation(GameState& state, std::size_t pawnIndex, lockstep::physics::BodyId body)
    {
        if (state.station && state.level && !body.isNull())
        {
            shootRangeTarget(*state.station, *state.level, pawnIndex, body);
        }
    }

    void runStationHostTick(GameState& state, EngineContext& engine)
    {
        if (!state.station || !state.level)
        {
            return;
        }

        StationMission& mission = *state.station;
        updateStationAsHost(mission, presentFeet(state, engine), engine.timeDomains.delta(world::TimeDomain::World),
            engine.timeDomains.delta(world::TimeDomain::Player));
        publishStationState(mission, engine);

        const Pawn& hostPawn = state.simulation.pawns[k_hostPawn];
        presentStation(state, engine, engine.entities.find(hostPawn.entity), hostPawn.controller, k_hostPawn);
    }

    void runStationClientTick(GameState& state, EngineContext& engine)
    {
        if (!state.station || !state.level)
        {
            return;
        }

        const Prediction& prediction = state.local.prediction;
        presentStation(state, engine, engine.entities.find(prediction.entity), prediction.controller, k_clientPawn);
    }

    bool receiveStationMessage(GameState& state, std::span<const std::byte> message)
    {
        const std::optional<StationState> received = decodeStationState(message);

        if (!received)
        {
            return false;
        }

        if (state.station)
        {
            state.station->state = *received;
        }

        return true;
    }

    void advanceStationForDev(GameState& state, EngineContext& engine)
    {
        if (!state.station || state.station->state.phase == StationPhase::MissionStart)
        {
            return;
        }

        StationMission& mission = *state.station;
        const std::array<std::optional<math::Vec3>, k_playerCount> feet = presentFeet(state, engine);
        std::clog << "Game: dev: advancing the station from phase " << toString(mission.state.phase) << '\n';

        switch (mission.state.phase)
        {
            case StationPhase::Report:
                startBriefing(mission, k_hostPawn);
                break;
            case StationPhase::Briefing:
                mission.briefingSeconds = 0.0f;
                break;
            case StationPhase::Armory:
                for (std::size_t player = 0; player < k_playerCount; ++player)
                {
                    if (feet[player])
                    {
                        giveArmoryItem(mission, player, ArmoryItem::Armor);
                        giveArmoryItem(mission, player, ArmoryItem::Weapon);
                        giveArmoryItem(mission, player, ArmoryItem::Ammo);
                        state.simulation.pawns[player].isArmed = true;
                    }
                }

                break;
            case StationPhase::GoToCar:
                mission.state.phase = StationPhase::MissionStart;
                std::clog << "Game: station: phase " << toString(mission.state.phase) << '\n';
                break;
            case StationPhase::MissionStart:
                break;
        }
    }
} //namespace game
