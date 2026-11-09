#ifndef LOCKSTEP_GAME_LEVEL_APARTMENT_ROOM_HPP
#define LOCKSTEP_GAME_LEVEL_APARTMENT_ROOM_HPP

#include "Echo/EchoRecording.hpp"
#include "Interaction/Interactables.hpp"
#include "Interaction/NodeMotion.hpp"
#include "Level/RoomState.hpp"
#include "Level/SceneLevel.hpp"

#include <Lockstep/Core/Math.hpp>
#include <Lockstep/Physics/PhysicsTypes.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <vector>

namespace lockstep::physics
{
    class PhysicsWorld;
} //namespace lockstep::physics

namespace game
{
    // A part of the room that swings on a hinge (a locker door, the front door).
    // motion: its instances and rest pose.
    // hinge:  the axis; positive angles open it.
    struct SwingingPart
    {
        NodeMotion motion;
        Hinge hinge;
    };

    // The opening room of the game: the shared flat and its morning routine. Every "Interact_*"
    // (and "Mug_*") node of the level is usable; the ones named in RoomState.cpp have rules.
    // The host runs the rules and owns `state`; the client mirrors it. Both apply the state to
    // their copy of the level the same way (moving doors and their colliders, hiding mugs).
    //
    // interactables:      usable nodes found in the level.
    // objects:            for each interactable, which room object it is.
    // alarm:              the alarm clock, shaken around a vertical axis while it rings.
    // lockers:            Player1's and Player2's locker doors.
    // frontDoor:          the front door.
    // mugs:               Mug_A's and Mug_B's instances.
    // state:              the shared state (authoritative on the host).
    // shownState:         the state the local player was last told about.
    // brewSeconds:        host: World seconds of brewing left.
    // doorWaitSeconds:    host: per player, Player seconds their door press stays valid.
    // sentState:          host: the encoded state last sent to the client.
    // shakeSeconds:       World seconds the alarm has rung, for its shake.
    // areMugsHidden:      which mugs this machine already hid.
    // lookTarget:         what the local player looks at.
    // prompt:             the prompt for it, empty for none.
    // recording:          host: the Echo recording being made.
    // isRecording:        host: recording until the front door has opened.
    // echoPath:           host: where the recording is saved.
    struct ApartmentRoom
    {
        std::vector<Interactable> interactables;
        std::vector<RoomObject> objects;
        std::optional<SwingingPart> alarm;
        std::array<std::optional<SwingingPart>, k_playerCount> lockers{};
        std::optional<SwingingPart> frontDoor;
        std::array<std::vector<uint32_t>, 2> mugs{};
        RoomState state{};
        RoomState shownState{};
        float brewSeconds{0.0f};
        std::array<float, k_playerCount> doorWaitSeconds{};
        std::optional<std::array<std::byte, k_roomStateBytes>> sentState;
        float shakeSeconds{0.0f};
        std::array<bool, 2> areMugsHidden{};
        std::optional<LookTarget> lookTarget;
        const char* prompt{""};
        EchoRecording recording{};
        bool isRecording{true};
        std::filesystem::path echoPath{k_defaultEchoPath};
    };

    // Finds the room's usable objects in a level and works out the hinges of its doors.
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
    // isPartnerJoined: whether the other player is in the game.
    void interactAsHost(ApartmentRoom& room, const SceneLevel& level, const lockstep::physics::PhysicsWorld& physics,
        std::size_t player, const math::Vec3& eye, const math::Vec3& direction,
        lockstep::physics::ControllerId ignore, bool isPartnerJoined);

    // Host: advances the room's timers and animations by one tick.
    // room:        the room.
    // worldDelta:  World domain delta (brewing and doors follow bullet-time).
    // playerDelta: Player domain delta (how long a door press waits for the partner).
    void updateRoomAsHost(ApartmentRoom& room, float worldDelta, float playerDelta);

    // Host: records one tick of both pawns for the Echo, and saves the recording once the front
    // door is fully open.
    // room:  the room.
    // frame: both pawns this tick.
    void recordEchoFrame(ApartmentRoom& room, const EchoFrame& frame);

    // Logs, for the local player, what changed in `state` since the last call (placeholder for UI
    // and audio).
    // room:        the room; its shown state catches up with `state`.
    // localPlayer: this machine's player index.
    void showRoomChanges(ApartmentRoom& room, std::size_t localPlayer);

    // Applies the state to this machine's level: shakes the ringing alarm, swings the lockers and
    // the front door (colliders too), hides taken mugs.
    // room:       the room.
    // level:      the level.
    // physics:    the physics world of the level's colliders.
    // worldDelta: World domain delta of this tick.
    void applyRoomState(ApartmentRoom& room, SceneLevel& level, lockstep::physics::PhysicsWorld& physics,
        float worldDelta);

    // Updates what the local player looks at and its prompt; logs the prompt when it changes
    // (placeholder until there is HUD text).
    // room:        the room.
    // level:       the level.
    // physics:     for the look raycast.
    // eye:         the local player's eyes.
    // direction:   unit look direction.
    // ignore:      the local pawn's controller.
    // localPlayer: this machine's player index.
    void updateLookPrompt(ApartmentRoom& room, const SceneLevel& level, const lockstep::physics::PhysicsWorld& physics,
        const math::Vec3& eye, const math::Vec3& direction, lockstep::physics::ControllerId ignore,
        std::size_t localPlayer);
} //namespace game

#endif //LOCKSTEP_GAME_LEVEL_APARTMENT_ROOM_HPP
