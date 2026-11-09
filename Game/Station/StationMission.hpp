#ifndef LOCKSTEP_GAME_STATION_STATION_MISSION_HPP
#define LOCKSTEP_GAME_STATION_STATION_MISSION_HPP

#include "Dialogue/Dialogue.hpp"
#include "Interaction/Interactables.hpp"
#include "Interaction/NodeMotion.hpp"
#include "Level/SceneLevel.hpp"
#include "Station/StationState.hpp"

#include <Lockstep/Core/Math.hpp>
#include <Lockstep/Physics/PhysicsTypes.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace lockstep::physics
{
    class PhysicsWorld;
} //namespace lockstep::physics

namespace game
{
    // How close to the car spot (horizontally, in metres) both players must be to leave.
    inline constexpr float k_stationCarReach{3.0f};

    // How close to the chief's desk (horizontally, in metres) both players must be for the
    // briefing to start, and how far above or below it (one storey).
    inline constexpr float k_chiefOfficeReach{4.0f};
    inline constexpr float k_chiefOfficeHeight{2.0f};

    // How close to the range targets (horizontally, in metres) an armed player hears the range
    // officer's introduction, and how far above or below them.
    inline constexpr float k_rangeIntroReach{18.0f};
    inline constexpr float k_rangeIntroHeight{3.0f};

    // Player seconds the host waits after the briefing's subtitles before the armory opens, so
    // the client's last subtitle is not cut off.
    inline constexpr float k_briefingTailSeconds{0.5f};

    // Player seconds the screen takes to fade to black once the mission starts.
    inline constexpr float k_missionFadeSeconds{2.0f};

    // The usable things of the station, found by node name.
    // Briefing: the chief's desk ("Interact_Briefing").
    // Armor, Weapon, Ammo: the armory counter ("Interact_ArmoryArmor" / "...Weapon" / "...Ammo").
    // Other:    an Interact_* node without rules.
    enum class StationObject : uint8_t
    {
        Briefing,
        Armor,
        Weapon,
        Ammo,
        Other
    };

    // A pop-up board on the range ("Target_<n>" node). Shot while standing, it tips over
    // backwards (away from the shooting line at +X) and stands up again after a while.
    // name:        the node's name.
    // instances:   its scene instances.
    // motion:      the instances' rest pose.
    // hinge:       the bottom edge it tips over on.
    // fall:        how far it lies down on this machine, 0 (up) to 1 (down).
    // downSeconds: host: World seconds until it stands up again.
    struct RangeTarget
    {
        std::string name;
        std::vector<uint32_t> instances;
        NodeMotion motion;
        Hinge hinge;
        float fall{0.0f};
        float downSeconds{0.0f};
    };

    // Level 1's police station: report to the chief, gear up at the armory, drive off (see
    // StationPhase); the range is optional. The host runs the rules and owns `state`; the client
    // mirrors it. Both animate the targets from the state.
    // interactables:   usable nodes ("Interact_*").
    // objects:         for each interactable, which station object it is.
    // targets:         the range targets, in node order (at most k_maxRangeTargets).
    // chiefDesk:       centre of the chief's desk (the "Interact_Briefing" node's bounds).
    // rangeSpot:       centre of the range targets' bottom edges, if the level has targets.
    // carSpot:         where the car waits ("CarSpot" node), if the level has one.
    // state:           the shared state (authoritative on the host).
    // shownState:      the state the local player was last told about.
    // sentState:       host: the encoded state last sent to the client.
    // briefingSeconds: host: Player seconds of the briefing left.
    // briefingLength:  Player seconds the chief's briefing takes (from the dialogue).
    // endSeconds:      Player seconds since the mission started, for the fade.
    // isRangeIntroduced: whether the local player heard the range officer's introduction.
    // lookTarget:      what the local player looks at.
    // prompt:          the prompt for it, empty for none.
    struct StationMission
    {
        std::vector<Interactable> interactables;
        std::vector<StationObject> objects;
        std::vector<RangeTarget> targets;
        math::Vec3 chiefDesk{};
        std::optional<math::Vec3> rangeSpot;
        std::optional<math::Vec3> carSpot;
        StationState state{};
        StationState shownState{};
        std::optional<std::array<std::byte, k_stationStateBytes>> sentState;
        float briefingSeconds{0.0f};
        float briefingLength{0.0f};
        float endSeconds{0.0f};
        bool isRangeIntroduced{false};
        std::optional<LookTarget> lookTarget;
        std::string prompt;
    };

    // Id prefix of the chief's briefing lines, and the id shown when there are none.
    inline constexpr std::string_view k_briefingPrefix{"station.chief.briefing."};
    inline constexpr std::string_view k_briefingFallback{"station.chief.briefing.01"};

    // Finds the station's usable objects, range targets and car spot.
    // level:    the loaded level.
    // dialogue: the level's dialogue (the briefing's length).
    // Returns:  the mission, or nothing if the level has no "Interact_Briefing" node.
    [[nodiscard]] std::optional<StationMission> createStationMission(const SceneLevel& level, DialogueBook& dialogue);

    // mission: the mission.
    // Returns: one bit per range target of the level.
    [[nodiscard]] uint8_t rangeTargetMask(const StationMission& mission);

    // Host: a player pressed E. Traces what they look at and applies the station's rules. The
    // briefing starts only while every present player is in the chief's office.
    // mission:   the mission.
    // level:     the level.
    // physics:   for the look raycast.
    // player:    the player index.
    // eye:       the player's eye position on the host.
    // direction: unit look direction.
    // ignore:    the player's controller.
    // feet:      per player, their pawn's foot position, or nothing while absent.
    // Returns:   what they took or used (StationObject::Weapon arms them), or nothing if they
    //            looked at nothing usable or were refused.
    [[nodiscard]] std::optional<StationObject> useStationAsHost(StationMission& mission, const SceneLevel& level,
        const lockstep::physics::PhysicsWorld& physics, std::size_t player, const math::Vec3& eye,
        const math::Vec3& direction, lockstep::physics::ControllerId ignore,
        const std::array<std::optional<math::Vec3>, k_playerCount>& feet);

    // mission: the mission.
    // foot:    a pawn's foot position.
    // Returns: whether it stands within k_chiefOfficeReach of the chief's desk, on its floor.
    [[nodiscard]] bool isInChiefOffice(const StationMission& mission, const math::Vec3& foot);

    // Host: starts the briefing (phase Report only), without checking who is in the office.
    // mission: the mission.
    // player:  who reported.
    // Returns: true if the briefing started.
    bool startBriefing(StationMission& mission, std::size_t player);

    // Host: hands a player an armory item (from the Armory phase on).
    // mission: the mission.
    // player:  the player index.
    // item:    the item.
    // Returns: true if the player did not have it yet.
    bool giveArmoryItem(StationMission& mission, std::size_t player, ArmoryItem item);

    // Host: a player's shot struck a level body; knocks over the range target it belongs to if it
    // is standing.
    // mission: the mission.
    // level:   the level (its colliders map the body to an instance).
    // player:  the shooter.
    // body:    the struck body.
    // Returns: true if a target went down.
    bool shootRangeTarget(StationMission& mission, const SceneLevel& level, std::size_t player,
        lockstep::physics::BodyId body);

    // Host: knocks over a standing range target and credits the shooter.
    // mission: the mission.
    // target:  index into StationMission::targets.
    // player:  the shooter.
    // Returns: true if the target was standing.
    bool knockDownTarget(StationMission& mission, std::size_t target, std::size_t player);

    // Host: one tick of the station: targets stand up again, the briefing runs out, and the phase
    // moves on once every present player is done (geared, at the car).
    // mission:     the mission.
    // feet:        per player, their pawn's foot position, or nothing while absent.
    // worldDelta:  World domain delta (targets follow bullet-time).
    // playerDelta: Player domain delta (the briefing's subtitles).
    void updateStationAsHost(StationMission& mission, const std::array<std::optional<math::Vec3>, k_playerCount>& feet,
        float worldDelta, float playerDelta);

    // mission: the mission.
    // foot:    a pawn's foot position.
    // Returns: whether it stands within k_stationCarReach of the car spot (false without one).
    [[nodiscard]] bool isNearStationCar(const StationMission& mission, const math::Vec3& foot);

    // Both machines: tips the range targets towards the state (colliders too).
    // mission:    the mission.
    // level:      the level.
    // physics:    the physics world of the level's colliders.
    // worldDelta: World domain delta of this tick.
    void applyStationState(StationMission& mission, SceneLevel& level, lockstep::physics::PhysicsWorld& physics,
        float worldDelta);

    // Updates what the local player looks at and its prompt.
    // mission:   the mission.
    // level:     the level.
    // physics:   for the look raycast.
    // eye:       the local player's eyes.
    // direction: unit look direction.
    // ignore:    the local pawn's controller.
    // player:    the local player's index.
    void updateStationLook(StationMission& mission, const SceneLevel& level,
        const lockstep::physics::PhysicsWorld& physics, const math::Vec3& eye, const math::Vec3& direction,
        lockstep::physics::ControllerId ignore, std::size_t player);

    // The objective line for one player, from the dialogue's "station.objective.*" lines, with
    // progress counts.
    // mission:  the mission.
    // dialogue: the level's dialogue.
    // player:   the player index.
    // isAtCar:  whether the player stands by the car.
    // Returns:  the line, empty when there is nothing to do.
    [[nodiscard]] std::string stationObjective(const StationMission& mission, DialogueBook& dialogue,
        std::size_t player, bool isAtCar);

    // Both machines: whether the local player should now hear the range officer's introduction:
    // armed, near the range and not introduced yet. Marks it as introduced when true.
    // mission: the mission.
    // foot:    the local pawn's foot position.
    // player:  the local player's index.
    // Returns: true once, when the introduction should play.
    [[nodiscard]] bool takeRangeIntroduction(StationMission& mission, const math::Vec3& foot, std::size_t player);

    // Both machines: counts the mission start's fade and logs the mission start once.
    // mission:     the mission.
    // playerDelta: Player domain delta of this tick.
    void stepMissionStart(StationMission& mission, float playerDelta);

    // mission: the mission.
    // Returns: how far the screen has faded out for the mission start, 0 to 1.
    [[nodiscard]] float missionStartFade(const StationMission& mission);
} //namespace game

#endif //LOCKSTEP_GAME_STATION_STATION_MISSION_HPP
