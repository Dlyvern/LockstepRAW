#ifndef LOCKSTEP_GAME_LEVEL_ROOM_STATE_HPP
#define LOCKSTEP_GAME_LEVEL_ROOM_STATE_HPP

#include "Player/Pawns.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

namespace game
{
    // RoomState player fields meaning "no player".
    inline constexpr uint8_t k_nobody{0xFF};

    // Most toys a level may have (RoomState::heldToy indexes them; findToys() keeps no more).
    inline constexpr std::size_t k_maxToys{64};

    // The things in the apartment and the district that have rules, found by node name (see
    // roomObjectFor()).
    // Gear1/Gear2: the badge and sidearm in Player1's / Player2's locker ("Interact_Gear_1/2").
    // Toy:   a physics toy (Toy_* node, see Toys.hpp).
    // Other: an Interact_* node without rules; using it only logs a line.
    enum class RoomObject : uint8_t
    {
        AlarmClock,
        CoffeeMachine,
        MugA,
        MugB,
        Radio,
        Mirror,
        Locker1,
        Locker2,
        Gear1,
        Gear2,
        FrontDoor,
        WallClock,
        StreetDoor,
        Toy,
        Other
    };

    // What a successful or refused interaction did, for logs and the Echo recording.
    enum class RoomAction : uint8_t
    {
        SilenceAlarm,
        StartBrew,
        TakeMug,
        OpenLocker,
        CloseLocker,
        TakeGear,
        ToggleRadio,
        UseMirror,
        OpenFrontDoor,
        OpenStreetDoor,
        CloseStreetDoor,
        PickUpToy,
        ThrowToy,
        DropToy,
        StartDrive,
        Inspect,
        Refused
    };

    // Coffee machine: Idle until someone starts it, Brewing for a while (World time), then Ready.
    enum class CoffeePhase : uint8_t
    {
        Idle,
        Brewing,
        Ready
    };

    // The morning routine, start to end.
    // Apartment:       waking up and gearing up in the flat, until the front door opens.
    // GoToCar:         down the stairs, out of the street door, across the park to the car.
    // MorningComplete: both players reached the car; the drive to the station starts.
    enum class MorningPhase : uint8_t
    {
        Apartment,
        GoToCar,
        MorningComplete
    };

    // Front door: Closed until a geared player opens it with their geared partner close by, Opening
    // while it swings, then Open.
    enum class DoorPhase : uint8_t
    {
        Closed,
        Opening,
        Open
    };

    // A line the host shows one player about their own interaction (why it was refused, what
    // they see). Delivered by bumping RoomState::noticeCount for that player.
    enum class RoomNotice : uint8_t
    {
        None,
        AlarmAlreadyOff,
        StillBrewing,
        CoffeeNotReady,
        AlreadyHaveCoffee,
        NotYourLocker,
        NeedGear,
        PartnerNeedsGear,
        PartnerAbsent,
        MirrorLook,
        WallClockTime,
        NothingHappens
    };

    // Bytes an encoded RoomState message takes.
    inline constexpr std::size_t k_roomStateBytes{20};

    // The morning's shared state (the flat and the district). The host owns it and sends it to the
    // client (reliable Game message) whenever it changes; both apply it to the level the same way.
    // isAlarmRinging:  the alarm clock rings until someone silences it.
    // alarmSilencedBy: the player index who silenced it, or k_nobody.
    // coffee:          coffee machine phase.
    // mugTakenBy:      for Mug_A and Mug_B, the player who took it, or k_nobody.
    // isLockerOpen:    each player's locker door is open or opening (anyone may toggle it).
    // lockerOpen:      each player's locker door, 0 closed to 1 fully open (animated both ways).
    // hasBadge:        per player; their Interact_Gear_* cube is hidden once taken.
    // hasSidearm:      per player.
    // isRadioOn:       the radio plays (audio later).
    // hasUsedMirror:   per player (stub until mirrors render).
    // door:            front door phase.
    // doorOpen:        front door, 0 closed to 1 fully open (animated).
    // phase:           where the morning routine is.
    // isStreetDoorOpen: the lobby's street door is open or opening.
    // streetDoorOpen:  street door, 0 closed to 1 fully open (animated both ways).
    // streetDoorBy:    the player who last opened or closed the street door, or k_nobody.
    // heldToy:         per player, the index of the toy they carry, or k_nobody.
    // noticeCount:     per player, bumped with every new notice.
    // notice:          per player, the newest notice.
    struct RoomState
    {
        bool isAlarmRinging{true};
        uint8_t alarmSilencedBy{k_nobody};
        CoffeePhase coffee{CoffeePhase::Idle};
        std::array<uint8_t, 2> mugTakenBy{k_nobody, k_nobody};
        std::array<bool, k_playerCount> isLockerOpen{};
        std::array<float, k_playerCount> lockerOpen{};
        std::array<bool, k_playerCount> hasBadge{};
        std::array<bool, k_playerCount> hasSidearm{};
        bool isRadioOn{false};
        std::array<bool, k_playerCount> hasUsedMirror{};
        DoorPhase door{DoorPhase::Closed};
        float doorOpen{0.0f};
        MorningPhase phase{MorningPhase::Apartment};
        bool isStreetDoorOpen{false};
        float streetDoorOpen{0.0f};
        uint8_t streetDoorBy{k_nobody};
        std::array<uint8_t, k_playerCount> heldToy{k_nobody, k_nobody};
        std::array<uint8_t, k_playerCount> noticeCount{};
        std::array<RoomNotice, k_playerCount> notice{};
    };

    // name:    an interactable's node name.
    // Returns: which room object it is (Other for an unknown name).
    [[nodiscard]] RoomObject roomObjectFor(std::string_view name);

    // player:  a player index.
    // Returns: "Player1" or "Player2".
    [[nodiscard]] const char* playerName(std::size_t player);

    // action:  an action.
    // Returns: its name, for logs.
    [[nodiscard]] const char* toString(RoomAction action);

    // object:  a room object.
    // Returns: its name, for logs.
    [[nodiscard]] const char* toString(RoomObject object);

    // The short prompt shown while a player looks at something ("E: Silence the alarm"). Toys
    // have their own prompt (see toyPrompt()).
    // state:  the room state.
    // object: what the player looks at.
    // player: the looking player.
    // Returns: the prompt text.
    [[nodiscard]] const char* promptText(const RoomState& state, RoomObject object, std::size_t player);

    // Describes, for one player, what changed between two room states: the lines that player
    // should read on screen.
    // before: the previous state.
    // after:  the new state.
    // player: the local player.
    // Returns: the dialogue ids of the lines ("morning.*", see lineText()), in order.
    [[nodiscard]] std::vector<std::string_view> describeRoomChanges(const RoomState& before, const RoomState& after,
        std::size_t player);

    // Packs the state into a Game message (animations quantized to 1/255).
    // state:   the room state.
    // Returns: the message bytes.
    [[nodiscard]] std::array<std::byte, k_roomStateBytes> encodeRoomState(const RoomState& state);

    // Unpacks a Game message.
    // bytes:   a received Game message.
    // Returns: the state, or nothing if `bytes` is not a valid room state message (a player or
    //          toy index out of range included).
    [[nodiscard]] std::optional<RoomState> decodeRoomState(std::span<const std::byte> bytes);
} //namespace game

#endif //LOCKSTEP_GAME_LEVEL_ROOM_STATE_HPP
