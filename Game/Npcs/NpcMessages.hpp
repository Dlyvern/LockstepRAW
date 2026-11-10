#ifndef LOCKSTEP_GAME_NPCS_NPC_MESSAGES_HPP
#define LOCKSTEP_GAME_NPCS_NPC_MESSAGES_HPP

#include "Characters/GearLooks.hpp"
#include "Npcs/Npc.hpp"

#include <Lockstep/Anim/Limbs.hpp>
#include <Lockstep/World/Entities.hpp>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace game
{
    // What the client needs to draw one NPC besides its replicated transform.
    // entity:     the NPC's entity on the host (MirroredEntity::hostEntity on the client).
    // isDead:     it is a body (drawn falling over, then lying still).
    // isAlert:    alert or fighting (drawn with the weapon up), else relaxed.
    // shotCount:  shots fired, wrapping; each increase flashes its muzzle.
    // gearLook:   its head gear (from its marker name, so both machines draw the same).
    // body:       its character model (squadBody() of its marker name, mercBody() for mercs).
    // hitHeading: heading the killing shot travelled along, in 256 steps of a full turn
    //             (seeds the body's ragdoll push); nothing if unknown.
    // hitLimb:    the limb the killing shot hit (its ragdoll body is pushed); None if unknown.
    struct NpcView
    {
        lockstep::world::EntityId entity{};
        bool isDead{false};
        bool isAlert{false};
        uint8_t shotCount{0};
        GearLook gearLook{GearLook::Full};
        NpcBody body{NpcBody::Swat};
        std::optional<uint8_t> hitHeading{};
        lockstep::anim::Limb hitLimb{lockstep::anim::Limb::None};

        friend bool operator==(const NpcView&, const NpcView&) = default;
    };

    // The host's reliable message of every NPC's view, sent whenever one changes.
    struct NpcStateMessage
    {
        std::vector<NpcView> npcs;

        friend bool operator==(const NpcStateMessage&, const NpcStateMessage&) = default;
    };

    // A callout, sent reliably to the client.
    // speaker: the speaking NPC's entity on the host.
    // id:      dialogue line id (at most 255 bytes).
    struct NpcCalloutMessage
    {
        lockstep::world::EntityId speaker{};
        std::string id{};
    };

    // npc:     a host NPC.
    // Returns: what the client sees of it.
    [[nodiscard]] NpcView npcView(const Npc& npc);

    // view:    an NPC's view.
    // Returns: the horizontal unit direction its killing shot travelled along, or nothing.
    [[nodiscard]] std::optional<math::Vec3> hitDirection(const NpcView& view);

    // Packs the NPC state message (at most 255 NPCs are sent).
    // message: the views.
    // Returns: the message bytes.
    [[nodiscard]] std::vector<std::byte> encodeNpcState(const NpcStateMessage& message);

    // Unpacks a Game message.
    // bytes:   a received Game message.
    // Returns: the NPC state message, or nothing if `bytes` is not one.
    [[nodiscard]] std::optional<NpcStateMessage> decodeNpcState(std::span<const std::byte> bytes);

    // Packs a callout.
    // message: the callout.
    // Returns: the message bytes.
    [[nodiscard]] std::vector<std::byte> encodeNpcCallout(const NpcCalloutMessage& message);

    // Unpacks a Game message.
    // bytes:   a received Game message.
    // Returns: the callout, or nothing if `bytes` is not one.
    [[nodiscard]] std::optional<NpcCalloutMessage> decodeNpcCallout(std::span<const std::byte> bytes);
} //namespace game

#endif //LOCKSTEP_GAME_NPCS_NPC_MESSAGES_HPP
