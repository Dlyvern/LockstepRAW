#ifndef LOCKSTEP_GAME_LEVEL_APARTMENT_ROOM_HPP
#define LOCKSTEP_GAME_LEVEL_APARTMENT_ROOM_HPP

#include "Echo/EchoRecording.hpp"
#include "Interaction/Interactables.hpp"
#include "Interaction/NodeMotion.hpp"
#include "Interaction/Toys.hpp"
#include "Level/RoomState.hpp"
#include "Level/SceneLevel.hpp"

#include <Lockstep/Core/Math.hpp>
#include <Lockstep/Physics/PhysicsTypes.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace lockstep::physics
{
    class PhysicsWorld;
} //namespace lockstep::physics

namespace game
{
    // A part of the room that swings on a hinge (a locker door, the front door, the street door).
    // motion:      its instances and rest pose.
    // hinge:       the axis.
    // closedAngle: radians from the rest pose when closed (0 for parts authored closed).
    // openAngle:   radians from the rest pose when fully open (0 for parts authored open).
    struct SwingingPart
    {
        NodeMotion motion;
        Hinge hinge;
        float closedAngle{0.0f};
        float openAngle{0.0f};
    };

    // The opening of the game: the shared flat, and in the district level the stairs, the street
    // door, the park and the car: the morning routine (see MorningPhase). Every "Interact_*" (and
    // "Mug_*") node of the level is usable; the ones named in RoomState.cpp have rules. Balls
    // ("Toy_*ball*") are physics toys. The host runs the rules and owns `state`; the client
    // mirrors it. Both apply the state to their copy of the level the same way (moving doors and
    // their colliders, hiding mugs).
    //
    // interactables:      usable nodes found in the level.
    // objects:            for each interactable, which room object it is.
    // alarm:              the alarm clock, shaken around a vertical axis while it rings.
    // lockers:            Player1's and Player2's locker doors.
    // frontDoor:          the front door.
    // frontDoorSpot:      the centre of the front door, for the partner's distance to it.
    // streetDoor:         the lobby's street door (district level).
    // toys:               the balls.
    // carSpot:            where the car waits ("CarSpot" node), if the level has one.
    // mugs:               Mug_A's and Mug_B's instances.
    // gear:               the instances of the gear cubes in Player1's and Player2's lockers.
    // state:              the shared state (authoritative on the host).
    // shownState:         the state the local player was last told about.
    // brewSeconds:        host: World seconds of brewing left.
    // driveSeconds:       Player seconds since the morning completed, for the fade to black.
    // sentState:          host: the encoded state last sent to the client.
    // shakeSeconds:       World seconds the alarm has rung, for its shake.
    // areMugsHidden:      which mugs this machine already hid.
    // isGearHidden:       which gear cubes this machine already hid.
    // lookTarget:         what the local player looks at.
    // lookToy:            the toy the local player looks at.
    // prompt:             the prompt for what the local player looks at or carries, empty for none.
    // recording:          host: the Echo recording being made.
    // isRecording:        host: recording until the morning is complete (in a level without a
    //                     car, until the front door has opened). Never on a client.
    // echoPath:           host: where the recording is saved.
    struct ApartmentRoom
    {
        std::vector<Interactable> interactables;
        std::vector<RoomObject> objects;
        std::optional<SwingingPart> alarm;
        std::array<std::optional<SwingingPart>, k_playerCount> lockers{};
        std::optional<SwingingPart> frontDoor;
        std::optional<math::Vec3> frontDoorSpot;
        std::optional<SwingingPart> streetDoor;
        std::vector<Toy> toys;
        std::optional<math::Vec3> carSpot;
        std::array<std::vector<uint32_t>, 2> mugs{};
        std::array<std::vector<uint32_t>, k_playerCount> gear{};
        RoomState state{};
        RoomState shownState{};
        float brewSeconds{0.0f};
        float driveSeconds{0.0f};
        std::optional<std::array<std::byte, k_roomStateBytes>> sentState;
        float shakeSeconds{0.0f};
        std::array<bool, 2> areMugsHidden{};
        std::array<bool, k_playerCount> isGearHidden{};
        std::optional<LookTarget> lookTarget;
        std::optional<std::size_t> lookToy;
        std::string prompt;
        EchoRecording recording{};
        bool isRecording{true};
        std::filesystem::path echoPath;
    };

    // Finds the room's usable objects, toys and car spot in a level and works out the hinges of
    // its doors. The street door is authored standing open; its hinge is its west edge.
    // level:  the loaded level.
    // spawns: where the players start (the front door swings away from them, out of the flat).
    // Returns: the room, or nothing if the level has no usable nodes (another level, or one cooked
    //          without the node hierarchy; a line says which).
    [[nodiscard]] std::optional<ApartmentRoom> createApartmentRoom(const SceneLevel& level, const LevelSpawns& spawns);

    // Host: a player pressed E. Traces what they look at and applies the room's rules; records the
    // interaction for the Echo.
    // room:      the room.
    // level:     the level.
    // physics:   for the look raycast.
    // player:    the player index.
    // eye:       the player's eye position on the host.
    // direction: the player's unit look direction.
    // ignore:    the player's controller.
    // partnerFoot: the other player's foot position, or nothing while they are not in the game
    //              (the front door opens only with the partner geared and within 3 m of it).
    // Returns:   what the interaction did (RoomAction::TakeGear arms the player), or nothing if
    //            they looked at nothing usable.
    [[nodiscard]] std::optional<RoomAction> interactAsHost(ApartmentRoom& room, const SceneLevel& level,
        const lockstep::physics::PhysicsWorld& physics, std::size_t player, const math::Vec3& eye,
        const math::Vec3& direction, lockstep::physics::ControllerId ignore, const std::optional<math::Vec3>& partnerFoot);

    // Host: adds an interaction to the Echo recording while it records.
    // room:   the room.
    // player: who interacted.
    // object: what with.
    // action: what it did.
    void recordEchoEvent(ApartmentRoom& room, std::size_t player, RoomObject object, RoomAction action);

    // Host: advances the room's timers and animations by one tick (lockers and the street door
    // both ways).
    // room:       the room.
    // worldDelta: World domain delta (brewing and doors follow bullet-time).
    void updateRoomAsHost(ApartmentRoom& room, float worldDelta);

    // Host: records one tick of both pawns for the Echo, and saves the recording once the morning
    // is complete (or, in a level without a car, once the front door is fully open).
    // room:  the room.
    // frame: both pawns this tick.
    void recordEchoFrame(ApartmentRoom& room, const EchoFrame& frame);

    // Lists, for the local player, what changed in `state` since the last call.
    // room:        the room; its shown state catches up with `state`.
    // localPlayer: this machine's player index.
    // Returns:     the dialogue ids of the lines to show, in order.
    [[nodiscard]] std::vector<std::string_view> takeRoomChanges(ApartmentRoom& room, std::size_t localPlayer);

    // Applies the state to this machine's level: shakes the ringing alarm, swings the lockers, the
    // front door and the street door (colliders too), hides taken mugs and gear.
    // room:       the room.
    // level:      the level.
    // physics:    the physics world of the level's colliders.
    // worldDelta: World domain delta of this tick.
    void applyRoomState(ApartmentRoom& room, SceneLevel& level, lockstep::physics::PhysicsWorld& physics,
        float worldDelta);

    // Updates what the local player looks at (usable things and toys) and its prompt.
    // room:        the room.
    // level:       the level.
    // physics:     for the look raycast.
    // entities:    the entity store holding the toys' entities.
    // eye:         the local player's eyes.
    // direction:   unit look direction.
    // ignore:      the local pawn's controller.
    // localPlayer: this machine's player index.
    void updateLookPrompt(ApartmentRoom& room, const SceneLevel& level, const lockstep::physics::PhysicsWorld& physics,
        const lockstep::world::Entities& entities, const math::Vec3& eye, const math::Vec3& direction,
        lockstep::physics::ControllerId ignore, std::size_t localPlayer);
} //namespace game

#endif //LOCKSTEP_GAME_LEVEL_APARTMENT_ROOM_HPP
