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

    // The things in the apartment that have rules, found by node name (see roomObjectFor()).
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
        FrontDoor,
        WallClock,
        Other
    };

    // What a successful or refused interaction did, for logs and the Echo recording.
    enum class RoomAction : uint8_t
    {
        SilenceAlarm,
        StartBrew,
        TakeMug,
        OpenLocker,
        TakeGear,
        ToggleRadio,
        UseMirror,
        ReadyAtDoor,
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

    // Front door: Closed until both players are ready at it, Opening while it swings, then Open.
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
        LockerEmpty,
        NeedGear,
        PartnerNeedsGear,
        WaitingForPartner,
        PartnerAbsent,
        MirrorLook,
        WallClockTime,
        NothingHappens
    };

    // Bytes an encoded RoomState message takes.
    inline constexpr std::size_t k_roomStateBytes{15};

    // The apartment's shared state. The host owns it and sends it to the client (reliable Game
    // message) whenever it changes; both apply it to the level the same way.
    // isAlarmRinging:  the alarm clock rings until someone silences it.
    // alarmSilencedBy: the player index who silenced it, or k_nobody.
    // coffee:          coffee machine phase.
    // mugTakenBy:      for Mug_A and Mug_B, the player who took it, or k_nobody.
    // lockerOpen:      each player's locker door, 0 closed to 1 fully open (animated).
    // hasBadge:        per player.
    // hasSidearm:      per player.
    // isRadioOn:       the radio plays (audio later).
    // hasUsedMirror:   per player (stub until mirrors render).
    // isWaitingAtDoor: per player: pressed E at the front door within the last few seconds.
    // door:            front door phase.
    // doorOpen:        front door, 0 closed to 1 fully open (animated).
    // noticeCount:     per player, bumped with every new notice.
    // notice:          per player, the newest notice.
    struct RoomState
    {
        bool isAlarmRinging{true};
        uint8_t alarmSilencedBy{k_nobody};
        CoffeePhase coffee{CoffeePhase::Idle};
        std::array<uint8_t, 2> mugTakenBy{k_nobody, k_nobody};
        std::array<float, k_playerCount> lockerOpen{};
        std::array<bool, k_playerCount> hasBadge{};
        std::array<bool, k_playerCount> hasSidearm{};
        bool isRadioOn{false};
        std::array<bool, k_playerCount> hasUsedMirror{};
        std::array<bool, k_playerCount> isWaitingAtDoor{};
        DoorPhase door{DoorPhase::Closed};
        float doorOpen{0.0f};
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

    // The short prompt shown while a player looks at something ("E: Silence the alarm").
    // state:  the room state.
    // object: what the player looks at.
    // player: the looking player.
    // Returns: the prompt text.
    [[nodiscard]] const char* promptText(const RoomState& state, RoomObject object, std::size_t player);

    // Describes, for one player, what changed between two room states: the lines that player
    // should read (placeholder for UI and audio).
    // before: the previous state.
    // after:  the new state.
    // player: the local player.
    // Returns: the lines, in order.
    [[nodiscard]] std::vector<const char*> describeRoomChanges(const RoomState& before, const RoomState& after,
        std::size_t player);

    // Packs the state into a Game message (animations quantized to 1/255).
    // state:   the room state.
    // Returns: the message bytes.
    [[nodiscard]] std::array<std::byte, k_roomStateBytes> encodeRoomState(const RoomState& state);

    // Unpacks a Game message.
    // bytes:   a received Game message.
    // Returns: the state, or nothing if `bytes` is not a room state message.
    [[nodiscard]] std::optional<RoomState> decodeRoomState(std::span<const std::byte> bytes);
} //namespace game

#endif //LOCKSTEP_GAME_LEVEL_ROOM_STATE_HPP
