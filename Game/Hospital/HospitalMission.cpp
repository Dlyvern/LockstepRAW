#include "Hospital/HospitalMission.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <iostream>

namespace game
{
    namespace
    {
        namespace assets = lockstep::assets;

        constexpr std::string_view k_beatPrefix{"Beat_"};
        constexpr std::string_view k_mercPrefix{"Spawn_Merc_"};
        constexpr std::string_view k_squadPrefix{"Npc_Squad_"};
        constexpr std::string_view k_liftNode{"Interact_ServiceLift"};
        constexpr std::string_view k_groundBeat{"Beat_GroundFloor"};
        constexpr std::byte k_hospitalTag{0x41};
        constexpr std::byte k_leavingBit{0x01};
        constexpr float k_floorHeight{4.0f};
        constexpr math::Vec3 k_nodeForward{0.0f, 0.0f, -1.0f};
        constexpr std::size_t k_maxBeatBits{16};

        uint8_t floorOf(std::string_view name, float height)
        {
            if (name.find("_G_") != std::string_view::npos)
            {
                return 0;
            }

            if (name.find("_F2") != std::string_view::npos)
            {
                return 1;
            }

            if (name.find("_F3") != std::string_view::npos)
            {
                return 2;
            }

            const auto floor = static_cast<int32_t>(std::lround(height / k_floorHeight));
            return static_cast<uint8_t>(std::clamp(floor, 0, static_cast<int32_t>(k_mercFloors) - 1));
        }

        bool isSquadMarker(std::string_view name)
        {
            return name.starts_with(k_squadPrefix) && name.size() > k_squadPrefix.size()
                && std::isdigit(static_cast<unsigned char>(name[k_squadPrefix.size()])) != 0;
        }

        HospitalMarker markerOf(const assets::SceneNode& node)
        {
            const math::Vec3 forward = node.transform.rotation * k_nodeForward;

            return HospitalMarker{.name = node.name,
                .foot = node.transform.position,
                .yaw = std::atan2(-forward.x, -forward.z),
                .floor = floorOf(node.name, node.transform.position.y)};
        }

        bool isReached(const HospitalMission& mission, std::string_view beat)
        {
            for (std::size_t index = 0; index < mission.beats.size() && index < k_maxBeatBits; ++index)
            {
                if (mission.beats[index].name.starts_with(beat)
                    && (mission.state.reachedBeats & (1u << index)) != 0)
                {
                    return true;
                }
            }

            return false;
        }
    } //namespace

    std::optional<HospitalMission> findHospitalMission(const assets::Scene& scene)
    {
        HospitalMission mission{};

        for (const assets::SceneNode& node : scene.nodes)
        {
            const std::string_view name = node.name;

            if (name.starts_with(k_beatPrefix))
            {
                mission.beats.push_back(markerOf(node));
            }
            else if (name.starts_with(k_mercPrefix))
            {
                mission.mercs.push_back(markerOf(node));
            }
            else if (isSquadMarker(name))
            {
                mission.squad.push_back(markerOf(node));
            }
        }

        if (mission.beats.empty() && mission.mercs.empty() && mission.squad.empty())
        {
            return std::nullopt;
        }

        if (const std::optional<math::Transform> lift = assets::findNode(scene, k_liftNode))
        {
            mission.lift = lift->position;
        }

        std::clog << "Game: hospital: " << mission.beats.size() << " beats, " << mission.mercs.size()
                  << " merc spawns, " << mission.squad.size() << " squad officers, lift "
                  << (mission.lift ? "yes" : "no") << '\n';

        return mission;
    }

    std::string_view hospitalObjectiveId(const HospitalMission& mission)
    {
        if (mission.state.isLeaving)
        {
            return {};
        }

        if (!isReached(mission, k_groundBeat))
        {
            return "hospital.objective.enter";
        }

        return "hospital.objective.basement";
    }

    std::array<std::byte, 4> encodeHospitalState(const HospitalState& state)
    {
        return {k_hospitalTag, static_cast<std::byte>(state.reachedBeats & 0xFFu),
            static_cast<std::byte>(state.reachedBeats >> 8), state.isLeaving ? k_leavingBit : std::byte{0}};
    }

    std::optional<HospitalState> decodeHospitalState(std::span<const std::byte> bytes)
    {
        if (bytes.size() != 4 || bytes[0] != k_hospitalTag)
        {
            return std::nullopt;
        }

        return HospitalState{
            .reachedBeats = static_cast<uint16_t>(static_cast<uint32_t>(bytes[1]) | (static_cast<uint32_t>(bytes[2]) << 8)),
            .isLeaving = (bytes[3] & k_leavingBit) != std::byte{0}};
    }
} //namespace game
