#ifndef LOCKSTEP_GAME_GORE_GORE_EVENTS_HPP
#define LOCKSTEP_GAME_GORE_GORE_EVENTS_HPP

#include <Lockstep/Anim/Limbs.hpp>
#include <Lockstep/Core/Math.hpp>
#include <Lockstep/World/Entities.hpp>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace game
{
    namespace math = lockstep::math;

    // What a gore event shows.
    // Hit:   a bullet hit flesh: a blood burst and splatter on the surfaces behind the hit.
    // Sever: a heavy hit took a limb off: the limb flies off as a piece, the stump bleeds.
    enum class GoreEventKind : uint8_t
    {
        Hit = 0,
        Sever
    };

    // GoreEvent::body of a hit on a living character (not on a ragdoll body).
    inline constexpr uint8_t k_noCorpseBody{0xFF};

    // GoreEvent::shooter when no player fired.
    inline constexpr uint8_t k_noShooter{0xFF};

    // One gore event, decided by the host and shown by both machines (presentation only; the
    // seed makes them look the same).
    // kind:      hit or sever.
    // target:    the hit character's entity on the host (an NPC or a pawn).
    // limb:      the severed limb (Sever only).
    // point:     world point of the hit.
    // direction: unit direction the shot travelled.
    // seed:      seed of the random look (splatter shapes, piece spin, droplets).
    // body:      for a bullet that hit a dead body: the ragdoll body it hit (each machine pushes
    //            that body of its own ragdoll); k_noCorpseBody for a hit on the living.
    // shooter:   the player whose shot it was (k_noShooter for NPC shots); that player's machine
    //            already showed the push and blood of a hit on a body.
    struct GoreEvent
    {
        GoreEventKind kind{GoreEventKind::Hit};
        lockstep::world::EntityId target{};
        lockstep::anim::Limb limb{lockstep::anim::Limb::None};
        math::Vec3 point{0.0f, 0.0f, 0.0f};
        math::Vec3 direction{0.0f, 0.0f, -1.0f};
        uint32_t seed{0};
        uint8_t body{k_noCorpseBody};
        uint8_t shooter{k_noShooter};
    };

    // A player's bullet that hit a dead body, as the shooter's machine traced it against its own
    // ragdolls. Sent by the client to the host (the host's own go straight into its queue); the
    // host decides whether it severs and publishes a GoreEvent.
    // target:    the dead NPC's entity on the host.
    // body:      the ragdoll body hit.
    // limb:      the limb that body belongs to.
    // point:     world point of the hit (on the shooter's machine).
    // direction: unit direction the shot travelled.
    struct CorpseHitReport
    {
        lockstep::world::EntityId target{};
        uint8_t body{0};
        lockstep::anim::Limb limb{lockstep::anim::Limb::None};
        math::Vec3 point{0.0f, 0.0f, 0.0f};
        math::Vec3 direction{0.0f, 0.0f, -1.0f};
    };

    // Host rules of when a hit severs a limb, editable from the dev panel (host).
    // severDamage:     a killing hit of at least this damage may sever the hit limb.
    // severChance:     chance it does, 0..1.
    // headshotSevers:  a killing hit on the head always takes it off.
    // npcSevers:       NPC shots (mercs on the squad) may sever too, with the same rules.
    struct GoreRules
    {
        int32_t severDamage{30};
        float severChance{0.6f};
        bool headshotSevers{true};
        bool npcSevers{true};
    };

    // Packs gore events into one Game message (at most 255 events).
    // events:  the events.
    // Returns: the message bytes.
    [[nodiscard]] std::vector<std::byte> encodeGoreEvents(std::span<const GoreEvent> events);

    // Unpacks a Game message.
    // bytes:   a received Game message.
    // Returns: the events, or nothing if `bytes` is not a gore message.
    [[nodiscard]] std::optional<std::vector<GoreEvent>> decodeGoreEvents(std::span<const std::byte> bytes);
    // Packs a corpse hit report into one Game message.
    // report:  the report.
    // Returns: the message bytes.
    [[nodiscard]] std::vector<std::byte> encodeCorpseHit(const CorpseHitReport& report);

    // Unpacks a Game message.
    // bytes:   a received Game message.
    // Returns: the report, or nothing if `bytes` is not a corpse hit report.
    [[nodiscard]] std::optional<CorpseHitReport> decodeCorpseHit(std::span<const std::byte> bytes);
} //namespace game

#endif //LOCKSTEP_GAME_GORE_GORE_EVENTS_HPP
