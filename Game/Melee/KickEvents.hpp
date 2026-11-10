#ifndef LOCKSTEP_GAME_MELEE_KICK_EVENTS_HPP
#define LOCKSTEP_GAME_MELEE_KICK_EVENTS_HPP

#include <Lockstep/Core/Math.hpp>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace lockstep::net
{
    class Session;
} //namespace lockstep::net

namespace game
{
    namespace math = lockstep::math;

    // One kick striking, decided by the host and shown by both machines: each pushes its own
    // ragdolls, severed limbs and dropped weapons (presentation physics), makes living NPCs flinch
    // and plays the thud.
    // pawn:      the kicking player's pawn index.
    // origin:    where the kick strikes from (the pawn's axis at kick height), world.
    // direction: unit planar direction of the kick.
    // isHit:     it struck something the host knows (an NPC, a toy, glass).
    struct KickEvent
    {
        uint8_t pawn{0};
        math::Vec3 origin{0.0f, 0.0f, 0.0f};
        math::Vec3 direction{0.0f, 0.0f, -1.0f};
        bool isHit{false};
    };

    // Packs a kick event (reliable Game message).
    // event:   the event.
    // Returns: the bytes.
    [[nodiscard]] std::vector<std::byte> encodeKickEvent(const KickEvent& event);

    // Unpacks a kick event.
    // bytes:   a received Game message.
    // Returns: the event, or nothing if `bytes` is not a kick event.
    [[nodiscard]] std::optional<KickEvent> decodeKickEvent(std::span<const std::byte> bytes);

    // Host, end of a tick: sends this tick's kicks to the client (when one is connected) and moves
    // them to the host's own presentation queue.
    // events:  this tick's kicks, emptied.
    // shown:   the host's presentation queue (KickEffects::pending).
    // session: the network session.
    void publishKickEvents(std::vector<KickEvent>& events, std::vector<KickEvent>& shown,
        lockstep::net::Session& session);
} //namespace game

#endif //LOCKSTEP_GAME_MELEE_KICK_EVENTS_HPP
