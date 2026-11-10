#include "Station/StationMission.hpp"

#include <Lockstep/Assets/SceneCollision.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <string_view>
#include <utility>

namespace game
{
    namespace
    {
        namespace assets = lockstep::assets;

        constexpr std::array<std::string_view, 1> k_interactPrefixes{"Interact_"};
        constexpr std::array<std::string_view, 1> k_targetPrefixes{"Target_"};
        constexpr std::string_view k_briefingNode{"Interact_Briefing"};
        constexpr std::string_view k_carSpotNode{"CarSpot"};
        constexpr std::array<std::pair<std::string_view, StationObject>, 4> k_objectNames{{
            {"Interact_Briefing", StationObject::Briefing},
            {"Interact_ArmoryArmor", StationObject::Armor},
            {"Interact_ArmoryWeapon", StationObject::Weapon},
            {"Interact_ArmoryAmmo", StationObject::Ammo}}};
        constexpr float k_hostReachTolerance{0.3f};
        constexpr float k_targetDownSeconds{3.0f};
        constexpr float k_targetFallSeconds{0.2f};
        constexpr float k_targetRiseSeconds{0.6f};
        constexpr float k_targetFallAngle{1.45f};
        constexpr math::Vec3 k_targetFallAxis{0.0f, 0.0f, 1.0f};

        StationObject stationObjectFor(std::string_view name)
        {
            for (const auto& [objectName, object] : k_objectNames)
            {
                if (objectName == name)
                {
                    return object;
                }
            }

            return StationObject::Other;
        }

        const char* playerLabel(std::size_t player)
        {
            return player == 0 ? "Player1" : "Player2";
        }

        void notify(StationState& state, std::size_t player, StationNotice notice)
        {
            ++state.noticeCount[player];
            state.notice[player] = notice;
        }

        std::vector<RangeTarget> findRangeTargets(const assets::Scene& scene)
        {
            std::vector<RangeTarget> targets;

            for (const Interactable& found : findInteractables(scene, k_targetPrefixes))
            {
                if (targets.size() >= k_maxRangeTargets)
                {
                    std::clog << "Game: station: more than " << k_maxRangeTargets << " range targets; '" << found.name
                        << "' is ignored\n";
                    continue;
                }

                const math::Aabb bounds = interactableBounds(scene, found);
                const math::Vec3 centre = math::center(bounds);
                targets.push_back(RangeTarget{.name = found.name,
                    .instances = found.instances,
                    .motion = captureNodeMotion(scene, found.instances),
                    .hinge = Hinge{.pivot = math::Vec3{centre.x, bounds.min.y, centre.z}, .axis = k_targetFallAxis}});
            }

            return targets;
        }

        bool everyPresent(const std::array<std::optional<math::Vec3>, k_playerCount>& feet, auto&& isDone)
        {
            bool anyPresent = false;

            for (std::size_t player = 0; player < k_playerCount; ++player)
            {
                if (!feet[player])
                {
                    continue;
                }

                anyPresent = true;

                if (!isDone(player, *feet[player]))
                {
                    return false;
                }
            }

            return anyPresent;
        }

        std::optional<StationObject> useArmory(StationMission& mission, std::size_t player, ArmoryItem item,
            StationObject object)
        {
            StationState& state = mission.state;

            if (state.phase == StationPhase::Report)
            {
                notify(state, player, StationNotice::ReportFirst);
                return std::nullopt;
            }

            if (state.phase == StationPhase::Briefing)
            {
                notify(state, player, StationNotice::ChiefTalking);
                return std::nullopt;
            }

            if (!giveArmoryItem(mission, player, item))
            {
                notify(state, player, StationNotice::AlreadyTaken);
                return std::nullopt;
            }

            return object;
        }

        std::optional<StationObject> applyRules(StationMission& mission, StationObject object, std::size_t player,
            const std::array<std::optional<math::Vec3>, k_playerCount>& feet)
        {
            StationState& state = mission.state;

            switch (object)
            {
                case StationObject::Briefing:
                    if (state.phase == StationPhase::Report
                        && !everyPresent(feet, [&mission](std::size_t, const math::Vec3& foot)
                            { return isInChiefOffice(mission, foot); }))
                    {
                        notify(state, player, StationNotice::WaitForPartner);
                        return std::nullopt;
                    }

                    if (startBriefing(mission, player))
                    {
                        return object;
                    }

                    notify(state, player, state.phase == StationPhase::Briefing ? StationNotice::ChiefTalking
                        : StationNotice::AlreadyBriefed);

                    return std::nullopt;
                case StationObject::Armor:
                    return useArmory(mission, player, ArmoryItem::Armor, object);
                case StationObject::Weapon:
                    return useArmory(mission, player, ArmoryItem::Weapon, object);
                case StationObject::Ammo:
                    return useArmory(mission, player, ArmoryItem::Ammo, object);
                case StationObject::Other:
                    break;
            }

            notify(state, player, StationNotice::NothingHappens);

            return std::nullopt;
        }

        void enterPhase(StationMission& mission, StationPhase phase)
        {
            mission.state.phase = phase;
            std::clog << "Game: station: phase " << toString(phase) << '\n';
        }

        void standTargetsUp(StationMission& mission, float worldDelta)
        {
            for (std::size_t index = 0; index < mission.targets.size(); ++index)
            {
                const auto bit = static_cast<uint8_t>(1u << index);
                RangeTarget& target = mission.targets[index];

                if ((mission.state.targetsDown & bit) == 0)
                {
                    continue;
                }

                target.downSeconds -= worldDelta;

                if (target.downSeconds <= 0.0f)
                {
                    mission.state.targetsDown = static_cast<uint8_t>(mission.state.targetsDown & ~bit);
                }
            }
        }

        std::string withCount(std::string_view text, int done, int total)
        {
            return std::string(text) + " (" + std::to_string(done) + "/" + std::to_string(total) + ")";
        }

        std::optional<math::Vec3> rangeCentre(const std::vector<RangeTarget>& targets)
        {
            if (targets.empty())
            {
                return std::nullopt;
            }

            math::Vec3 sum{};

            for (const RangeTarget& target : targets)
            {
                sum = sum + target.hinge.pivot;
            }

            return sum * (1.0f / static_cast<float>(targets.size()));
        }

        bool isWithin(const math::Vec3& foot, const math::Vec3& spot, float reach, float height)
        {
            const math::Vec3 offset = foot - spot;

            return math::length(math::Vec3{offset.x, 0.0f, offset.z}) <= reach && std::abs(offset.y) <= height;
        }

        int bitCount(uint8_t bits)
        {
            int count = 0;

            for (; bits != 0; bits = static_cast<uint8_t>(bits & (bits - 1)))
            {
                ++count;
            }

            return count;
        }

        const char* promptFor(const StationState& state, StationObject object, std::size_t player)
        {
            switch (object)
            {
                case StationObject::Briefing:
                    return state.phase == StationPhase::Report ? "F: Report to the chief" : "";
                case StationObject::Armor:
                    return hasArmoryItem(state, player, ArmoryItem::Armor) ? "" : "F: Take body armor";
                case StationObject::Weapon:
                    return hasArmoryItem(state, player, ArmoryItem::Weapon) ? "" : "F: Take a rifle";
                case StationObject::Ammo:
                    return hasArmoryItem(state, player, ArmoryItem::Ammo) ? "" : "F: Take ammunition";
                case StationObject::Other:
                    break;
            }

            return "F: Inspect";
        }
    } //namespace

    std::optional<StationMission> createStationMission(const SceneLevel& level, DialogueBook& dialogue)
    {
        const assets::Scene& scene = level.scene;

        if (!assets::findNodeIndex(scene, k_briefingNode))
        {
            return std::nullopt;
        }

        StationMission mission{};
        mission.interactables = findInteractables(scene, k_interactPrefixes);

        for (const Interactable& interactable : mission.interactables)
        {
            mission.objects.push_back(stationObjectFor(interactable.name));
            std::clog << "Game: station: usable '" << interactable.name << "' (" << interactable.instances.size()
                << " instances)\n";
        }

        mission.targets = findRangeTargets(scene);
        mission.rangeSpot = rangeCentre(mission.targets);

        for (const Interactable& interactable : mission.interactables)
        {
            if (interactable.name == k_briefingNode)
            {
                mission.chiefDesk = math::center(interactableBounds(scene, interactable));
            }
        }

        mission.briefingLength = linesSeconds(dialogue, k_briefingPrefix, k_briefingFallback);

        if (const std::optional<uint32_t> carSpot = assets::findNodeIndex(scene, k_carSpotNode))
        {
            mission.carSpot = scene.nodes[*carSpot].transform.position;
        }

        std::clog << "Game: station: " << mission.targets.size() << " range targets, briefing "
            << mission.briefingLength << " s, car spot " << (mission.carSpot ? "found" : "missing")
            << "; phase " << toString(mission.state.phase) << '\n';

        return mission;
    }

    uint8_t rangeTargetMask(const StationMission& mission)
    {
        return static_cast<uint8_t>((1u << mission.targets.size()) - 1u);
    }

    std::optional<StationObject> useStationAsHost(StationMission& mission, const SceneLevel& level,
        const lockstep::physics::PhysicsWorld& physics, std::size_t player, const math::Vec3& eye,
        const math::Vec3& direction, lockstep::physics::ControllerId ignore,
        const std::array<std::optional<math::Vec3>, k_playerCount>& feet)
    {
        const std::optional<LookTarget> target = traceInteractable(mission.interactables, level, physics, eye,
            direction, k_interactReach + k_hostReachTolerance, ignore);

        if (!target)
        {
            std::clog << "Game: station: " << playerLabel(player) << " pressed E at nothing\n";
            return std::nullopt;
        }

        const std::optional<StationObject> used = applyRules(mission, mission.objects[target->interactable], player,
            feet);
        std::clog << "Game: station: " << playerLabel(player) << " used "
            << mission.interactables[target->interactable].name << (used ? "" : " (refused)") << '\n';

        return used;
    }

    bool isInChiefOffice(const StationMission& mission, const math::Vec3& foot)
    {
        return isWithin(foot, mission.chiefDesk, k_chiefOfficeReach, k_chiefOfficeHeight);
    }

    bool startBriefing(StationMission& mission, std::size_t player)
    {
        if (mission.state.phase != StationPhase::Report)
        {
            return false;
        }

        mission.state.briefedBy = static_cast<uint8_t>(player);
        mission.briefingSeconds = mission.briefingLength + k_briefingTailSeconds;
        enterPhase(mission, StationPhase::Briefing);

        return true;
    }

    bool giveArmoryItem(StationMission& mission, std::size_t player, ArmoryItem item)
    {
        if (hasArmoryItem(mission.state, player, item))
        {
            return false;
        }

        mission.state.gear[player] = static_cast<uint8_t>(mission.state.gear[player] | static_cast<uint8_t>(item));
        std::clog << "Game: station: " << playerLabel(player) << " took armory item " << static_cast<int>(item)
            << " (gear bits " << static_cast<int>(mission.state.gear[player]) << ")\n";

        return true;
    }

    bool shootRangeTarget(StationMission& mission, const SceneLevel& level, std::size_t player,
        lockstep::physics::BodyId body)
    {
        const std::optional<uint32_t> instance = assets::findBodyInstance(level.collision, body);

        if (!instance)
        {
            return false;
        }

        for (std::size_t index = 0; index < mission.targets.size(); ++index)
        {
            const std::vector<uint32_t>& instances = mission.targets[index].instances;

            if (std::find(instances.begin(), instances.end(), *instance) != instances.end())
            {
                return knockDownTarget(mission, index, player);
            }
        }

        return false;
    }

    bool knockDownTarget(StationMission& mission, std::size_t target, std::size_t player)
    {
        const auto bit = static_cast<uint8_t>(1u << target);
        StationState& state = mission.state;

        if (target >= mission.targets.size() || (state.targetsDown & bit) != 0)
        {
            return false;
        }

        state.targetsDown = static_cast<uint8_t>(state.targetsDown | bit);
        state.targetsHit[player] = static_cast<uint8_t>(state.targetsHit[player] | bit);
        mission.targets[target].downSeconds = k_targetDownSeconds;

        std::clog << "Game: station: " << playerLabel(player) << " knocked down " << mission.targets[target].name
            << " (" << bitCount(state.targetsHit[player]) << "/" << mission.targets.size() << " this player)\n";

        return true;
    }

    void updateStationAsHost(StationMission& mission, const std::array<std::optional<math::Vec3>, k_playerCount>& feet,
        float worldDelta, float playerDelta)
    {
        StationState& state = mission.state;
        standTargetsUp(mission, worldDelta);

        switch (state.phase)
        {
            case StationPhase::Briefing:
                mission.briefingSeconds -= playerDelta;

                if (mission.briefingSeconds <= 0.0f)
                {
                    enterPhase(mission, StationPhase::Armory);
                }

                break;
            case StationPhase::Armory:
                if (everyPresent(feet, [&state](std::size_t player, const math::Vec3&)
                    { return state.gear[player] == k_allArmoryItems; }))
                {
                    enterPhase(mission, StationPhase::GoToCar);
                }

                break;
            case StationPhase::GoToCar:
                if (everyPresent(feet, [&mission](std::size_t, const math::Vec3& foot)
                    { return isNearStationCar(mission, foot); }))
                {
                    enterPhase(mission, StationPhase::MissionStart);
                }

                break;
            case StationPhase::Report:
            case StationPhase::MissionStart:
                break;
        }
    }

    bool isNearStationCar(const StationMission& mission, const math::Vec3& foot)
    {
        if (!mission.carSpot)
        {
            return false;
        }

        const math::Vec3 offset = foot - *mission.carSpot;

        return math::length(math::Vec3{offset.x, 0.0f, offset.z}) <= k_stationCarReach;
    }

    void applyStationState(StationMission& mission, SceneLevel& level, lockstep::physics::PhysicsWorld& physics,
        float worldDelta)
    {
        for (std::size_t index = 0; index < mission.targets.size(); ++index)
        {
            RangeTarget& target = mission.targets[index];
            const bool isDown = (mission.state.targetsDown & (1u << index)) != 0;
            target.fall = isDown ? std::min(target.fall + worldDelta / k_targetFallSeconds, 1.0f)
                : std::max(target.fall - worldDelta / k_targetRiseSeconds, 0.0f);
            rotateNodeMotion(target.motion, level, physics, target.hinge, k_targetFallAngle * target.fall);
        }
    }

    void updateStationLook(StationMission& mission, const SceneLevel& level,
        const lockstep::physics::PhysicsWorld& physics, const math::Vec3& eye, const math::Vec3& direction,
        lockstep::physics::ControllerId ignore, std::size_t player)
    {
        mission.lookTarget = traceInteractable(mission.interactables, level, physics, eye, direction, k_interactReach,
            ignore);
        mission.prompt = mission.lookTarget
            ? promptFor(mission.state, mission.objects[mission.lookTarget->interactable], player) : "";
    }

    std::string stationObjective(const StationMission& mission, DialogueBook& dialogue, std::size_t player,
        bool isAtCar)
    {
        const StationState& state = mission.state;

        switch (state.phase)
        {
            case StationPhase::Report:
                return std::string(lineText(dialogue, "station.objective.chief"));
            case StationPhase::Briefing:
                return std::string(lineText(dialogue, "station.objective.listen"));
            case StationPhase::Armory:
                if (state.gear[player] != k_allArmoryItems)
                {
                    return withCount(lineText(dialogue, "station.objective.armory"), bitCount(state.gear[player]),
                        bitCount(k_allArmoryItems));
                }

                [[fallthrough]];
            case StationPhase::GoToCar:
                return std::string(lineText(dialogue, isAtCar ? "station.objective.wait_car" : "station.objective.car"));
            case StationPhase::MissionStart:
                break;
        }

        return {};
    }

    bool takeRangeIntroduction(StationMission& mission, const math::Vec3& foot, std::size_t player)
    {
        if (mission.isRangeIntroduced || !mission.rangeSpot
            || !hasArmoryItem(mission.state, player, ArmoryItem::Weapon)
            || !isWithin(foot, *mission.rangeSpot, k_rangeIntroReach, k_rangeIntroHeight))
        {
            return false;
        }

        mission.isRangeIntroduced = true;

        return true;
    }

    void stepMissionStart(StationMission& mission, float playerDelta)
    {
        if (mission.state.phase != StationPhase::MissionStart)
        {
            return;
        }

        if (mission.endSeconds == 0.0f)
        {
            std::clog << "Game: station: MissionStart: both players are at the car; end of level 1\n";
        }

        mission.endSeconds += playerDelta;
    }

    float missionStartFade(const StationMission& mission)
    {
        return std::clamp(mission.endSeconds / k_missionFadeSeconds, 0.0f, 1.0f);
    }
} //namespace game
