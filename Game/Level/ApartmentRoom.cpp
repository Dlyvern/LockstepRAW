#include "Level/ApartmentRoom.hpp"

#include <array>
#include <cmath>
#include <iostream>
#include <string_view>

namespace game
{
    namespace
    {
        namespace assets = lockstep::assets;

        constexpr std::array<std::string_view, 2> k_interactPrefixes{"Interact_", "Mug_"};
        constexpr std::string_view k_carSpotNode{"CarSpot"};
        constexpr float k_lockerOpenAngle{1.745329f};
        constexpr float k_doorOpenAngle{1.658063f};
        constexpr float k_shakeAngle{0.06f};
        constexpr float k_shakeRate{55.0f};
        constexpr math::Vec3 k_worldUp{0.0f, 1.0f, 0.0f};

        float smoothStep(float value)
        {
            const float t = math::clamp(value, 0.0f, 1.0f);
            return t * t * (3.0f - 2.0f * t);
        }

        math::Vec3 parentPosition(const assets::Scene& scene, uint32_t node, const math::Vec3& fallback)
        {
            const uint32_t parent = scene.nodes[node].parent;
            return parent < scene.nodes.size() ? scene.nodes[parent].transform.position : fallback;
        }

        SwingingPart lockerPart(const assets::Scene& scene, const Interactable& door, const math::Vec3& roomCentre)
        {
            SwingingPart part{.motion = captureNodeMotion(scene, door.instances),
                .hinge = hingeFromBounds(scene, door.node, door.instances), .openAngle = k_lockerOpenAngle};
            orientHinge(part.hinge, scene, door.instances, parentPosition(scene, door.node, roomCentre));

            return part;
        }

        SwingingPart doorPart(const assets::Scene& scene, const Interactable& door, const math::Vec3& roomCentre)
        {
            SwingingPart part{.motion = captureNodeMotion(scene, door.instances),
                .hinge = hingeAtNode(scene, door.node), .openAngle = k_doorOpenAngle};
            orientHinge(part.hinge, scene, door.instances, roomCentre);

            return part;
        }

        SwingingPart streetDoorPart(const assets::Scene& scene, const Interactable& door)
        {
            return SwingingPart{.motion = captureNodeMotion(scene, door.instances),
                .hinge = hingeAtWestEdge(scene, door.node, door.instances), .closedAngle = -nodeYaw(scene, door.node)};
        }

        SwingingPart alarmPart(const assets::Scene& scene, const Interactable& alarm)
        {
            const math::Aabb bounds = interactableBounds(scene, alarm);
            const math::Vec3 centre = math::center(bounds);

            return SwingingPart{.motion = captureNodeMotion(scene, alarm.instances),
                .hinge = Hinge{.pivot = math::Vec3{centre.x, bounds.min.y, centre.z}, .axis = k_worldUp}};
        }

        void logPart(const char* what, const SwingingPart& part)
        {
            std::clog << "Game: room: " << what << " hinge at (" << part.hinge.pivot.x << ", " << part.hinge.pivot.y
                << ", " << part.hinge.pivot.z << "), axis (" << part.hinge.axis.x << ", " << part.hinge.axis.y << ", "
                << part.hinge.axis.z << "), " << part.motion.instances.size() << " instances\n";
        }

        void findParts(ApartmentRoom& room, const assets::Scene& scene, const math::Vec3& roomCentre)
        {
            for (std::size_t index = 0; index < room.interactables.size(); ++index)
            {
                const Interactable& interactable = room.interactables[index];

                switch (room.objects[index])
                {
                    case RoomObject::AlarmClock:
                        room.alarm = alarmPart(scene, interactable);
                        break;
                    case RoomObject::Locker1:
                        room.lockers[0] = lockerPart(scene, interactable, roomCentre);
                        logPart("Locker_1", *room.lockers[0]);
                        break;
                    case RoomObject::Locker2:
                        room.lockers[1] = lockerPart(scene, interactable, roomCentre);
                        logPart("Locker_2", *room.lockers[1]);
                        break;
                    case RoomObject::FrontDoor:
                        room.frontDoor = doorPart(scene, interactable, roomCentre);
                        room.frontDoorSpot = math::center(interactableBounds(scene, interactable));
                        logPart("FrontDoor", *room.frontDoor);
                        break;
                    case RoomObject::StreetDoor:
                        room.streetDoor = streetDoorPart(scene, interactable);
                        logPart("StreetDoor", *room.streetDoor);
                        break;
                    case RoomObject::MugA:
                        room.mugs[0] = interactable.instances;
                        break;
                    case RoomObject::MugB:
                        room.mugs[1] = interactable.instances;
                        break;
                    case RoomObject::Gear1:
                        room.gear[0] = interactable.instances;
                        break;
                    case RoomObject::Gear2:
                        room.gear[1] = interactable.instances;
                        break;
                    default:
                        break;
                }
            }
        }

        void swing(std::optional<SwingingPart>& part, SceneLevel& level, lockstep::physics::PhysicsWorld& physics,
            float open)
        {
            if (part)
            {
                const float angle = part->closedAngle + (part->openAngle - part->closedAngle) * smoothStep(open);
                rotateNodeMotion(part->motion, level, physics, part->hinge, angle);
            }
        }

        bool isRecordingDone(const ApartmentRoom& room)
        {
            return room.carSpot ? room.state.phase == MorningPhase::MorningComplete : room.state.door == DoorPhase::Open;
        }

        std::string toyPrompt(const ApartmentRoom& room, std::size_t localPlayer)
        {
            const uint8_t held = room.state.heldToy[localPlayer];

            if (held < room.toys.size())
            {
                return "LMB: Throw the " + room.toys[held].label + "    E: Drop it";
            }

            if (room.lookToy)
            {
                return "E: Pick up the " + room.toys[*room.lookToy].label;
            }

            return {};
        }

        void shakeAlarm(ApartmentRoom& room, SceneLevel& level, lockstep::physics::PhysicsWorld& physics,
            float worldDelta)
        {
            if (!room.alarm)
            {
                return;
            }

            float angle = 0.0f;

            if (room.state.isAlarmRinging)
            {
                room.shakeSeconds += worldDelta;
                angle = k_shakeAngle * std::sin(room.shakeSeconds * k_shakeRate);
            }

            rotateNodeMotion(room.alarm->motion, level, physics, room.alarm->hinge, angle);
        }
    } //namespace

    std::optional<ApartmentRoom> createApartmentRoom(const SceneLevel& level, const LevelSpawns& spawns)
    {
        const assets::Scene& scene = level.scene;
        ApartmentRoom room{};
        room.interactables = findInteractables(scene, k_interactPrefixes);

        if (room.interactables.empty())
        {
            if (assets::findNodeIndex(scene, "Interact"))
            {
                std::clog << "Game: room: the level has Interact_* nodes but no node hierarchy; re-cook it\n";
            }

            return std::nullopt;
        }

        for (const Interactable& interactable : room.interactables)
        {
            room.objects.push_back(roomObjectFor(interactable.name));
            std::clog << "Game: room: usable '" << interactable.name << "' (" << interactable.instances.size()
                << " instances)\n";
        }

        const math::Vec3 roomCentre = (spawns.pawns[0] + spawns.pawns[1]) * 0.5f;
        findParts(room, scene, roomCentre);
        room.toys = findToys(scene);

        if (const std::optional<uint32_t> carSpot = assets::findNodeIndex(scene, k_carSpotNode))
        {
            room.carSpot = scene.nodes[*carSpot].transform.position;
            std::clog << "Game: room: the car waits at (" << room.carSpot->x << ", " << room.carSpot->y << ", "
                << room.carSpot->z << ")\n";
        }

        std::clog << "Game: room: the alarm clock is ringing\n";

        return room;
    }

    void recordEchoFrame(ApartmentRoom& room, const EchoFrame& frame)
    {
        if (!room.isRecording)
        {
            return;
        }

        room.recording.frames.push_back(frame);

        if (!isRecordingDone(room))
        {
            return;
        }

        room.isRecording = false;

        if (auto saved = saveEchoRecording(room.recording, room.echoPath); !saved)
        {
            std::clog << "Game: echo not saved: " << saved.error().message << '\n';
            return;
        }

        std::clog << "Game: echo saved to '" << room.echoPath.string() << "': " << room.recording.frames.size()
            << " ticks, " << room.recording.events.size() << " interactions\n";
    }

    std::vector<const char*> takeRoomChanges(ApartmentRoom& room, std::size_t localPlayer)
    {
        std::vector<const char*> lines = describeRoomChanges(room.shownState, room.state, localPlayer);
        room.shownState = room.state;

        return lines;
    }

    void applyRoomState(ApartmentRoom& room, SceneLevel& level, lockstep::physics::PhysicsWorld& physics,
        float worldDelta)
    {
        shakeAlarm(room, level, physics, worldDelta);

        for (std::size_t player = 0; player < k_playerCount; ++player)
        {
            swing(room.lockers[player], level, physics, room.state.lockerOpen[player]);
        }

        swing(room.frontDoor, level, physics, room.state.doorOpen);
        swing(room.streetDoor, level, physics, room.state.streetDoorOpen);

        for (std::size_t mug = 0; mug < room.mugs.size(); ++mug)
        {
            if (!room.areMugsHidden[mug] && room.state.mugTakenBy[mug] != k_nobody)
            {
                setInstancesVisible(level, physics, room.mugs[mug], false);
                room.areMugsHidden[mug] = true;
            }
        }

        for (std::size_t player = 0; player < k_playerCount; ++player)
        {
            if (!room.isGearHidden[player] && room.state.hasBadge[player])
            {
                setInstancesVisible(level, physics, room.gear[player], false);
                room.isGearHidden[player] = true;
            }
        }
    }

    void updateLookPrompt(ApartmentRoom& room, const SceneLevel& level, const lockstep::physics::PhysicsWorld& physics,
        const lockstep::world::Entities& entities, const math::Vec3& eye, const math::Vec3& direction,
        lockstep::physics::ControllerId ignore, std::size_t localPlayer)
    {
        room.lookToy = traceToy(room.toys, entities, physics, room.state.heldToy, eye, direction, k_interactReach,
            ignore);
        room.lookTarget = room.lookToy ? std::nullopt
            : traceInteractable(room.interactables, level, physics, eye, direction, k_interactReach, ignore);
        room.prompt = toyPrompt(room, localPlayer);

        if (room.prompt.empty() && room.lookTarget)
        {
            room.prompt = promptText(room.state, room.objects[room.lookTarget->interactable], localPlayer);
        }
    }
} //namespace game
