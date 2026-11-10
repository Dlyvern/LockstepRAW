#ifndef LOCKSTEP_GAME_HOSPITAL_HOSPITAL_MISSION_HPP
#define LOCKSTEP_GAME_HOSPITAL_HOSPITAL_MISSION_HPP

#include <Lockstep/Assets/Scene.hpp>
#include <Lockstep/Core/Math.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace game
{
    namespace math = lockstep::math;

    // Merc floor groups: ground floor, second floor, third floor.
    inline constexpr std::size_t k_mercFloors{3};

    // The level Part A's service lift leads to.
    inline constexpr std::string_view k_hospitalNextLevel{"levels/hospital_b/scene"};

    // A marker the hospital reads from its level ("Beat_*", "Spawn_Merc_*", "Npc_Squad_*").
    // name:  the node's name.
    // foot:  its position.
    // yaw:   which way it faces (the node's -Z, Blender's +Y).
    // floor: 0 ground, 1 second, 2 third (from "_G_" / "_F2_" / "_F3_" in the name, else from
    //        its height at 4 m per floor).
    struct HospitalMarker
    {
        std::string name;
        math::Vec3 foot{0.0f, 0.0f, 0.0f};
        float yaw{0.0f};
        uint8_t floor{0};
    };

    // What the client needs of the hospital: which beats were reached (objective, checkpoint)
    // and whether the players are leaving by the lift (fade out). Sent reliably on change.
    // reachedBeats: bit per HospitalMission::beats entry (first 16).
    // isLeaving:    someone used the service lift.
    struct HospitalState
    {
        uint16_t reachedBeats{0};
        bool isLeaving{false};

        friend bool operator==(const HospitalState&, const HospitalState&) = default;
    };

    // Saint Abel Memorial Hospital, Part A: the cordon outside, the ground floor and two ward
    // floors with mercs, the service lift down. Mercs are opt-in for now (dev panel "Spawn
    // mercs", or `--dev-mercs`): with them enabled, a floor's group spawns when a player first
    // reaches a Beat_* on that floor (the ground floor's at once). Reaching a beat also makes it
    // the checkpoint. The two squad officers (Npc_Squad_1/2) spawn with the level and follow.
    // beats:          "Beat_*" markers in node order.
    // mercs:          "Spawn_Merc_*" markers.
    // squad:          "Npc_Squad_<n>" markers.
    // lift:           the "Interact_ServiceLift" node's position, if the level has one.
    // state:          host: the authority; client: as last received.
    // sentState:      host: the state last sent to the client.
    // spawnedFloors:  host: merc groups already spawned.
    // areMercsEnabled: host: beats spawn merc groups.
    // checkpointBeat: host: index into `beats` of the newest beat reached.
    // leaveSeconds:   Player seconds since the lift was used (fade out, then the next level).
    // isExitHandled:  the lift's end was handled (logged, or the next level asked for).
    struct HospitalMission
    {
        std::vector<HospitalMarker> beats;
        std::vector<HospitalMarker> mercs;
        std::vector<HospitalMarker> squad;
        std::optional<math::Vec3> lift;
        HospitalState state{};
        std::optional<HospitalState> sentState;
        std::array<bool, k_mercFloors> spawnedFloors{};
        bool areMercsEnabled{false};
        std::optional<std::size_t> checkpointBeat;
        float leaveSeconds{0.0f};
        bool isExitHandled{false};
    };

    // Reads the hospital markers of a level.
    // scene:   the loaded level.
    // Returns: the mission, or nothing when the level has no Beat_*, Spawn_Merc_* or Npc_Squad_*
    //          markers (not a hospital-style level).
    [[nodiscard]] std::optional<HospitalMission> findHospitalMission(const lockstep::assets::Scene& scene);

    // mission: the mission.
    // Returns: the dialogue id of the objective to show now.
    [[nodiscard]] std::string_view hospitalObjectiveId(const HospitalMission& mission);

    // Packs the hospital state into a Game message.
    // state:   the state.
    // Returns: the message bytes.
    [[nodiscard]] std::array<std::byte, 4> encodeHospitalState(const HospitalState& state);

    // Unpacks a Game message.
    // bytes:   a received Game message.
    // Returns: the state, or nothing if `bytes` is not one.
    [[nodiscard]] std::optional<HospitalState> decodeHospitalState(std::span<const std::byte> bytes);
} //namespace game

#endif //LOCKSTEP_GAME_HOSPITAL_HOSPITAL_MISSION_HPP
