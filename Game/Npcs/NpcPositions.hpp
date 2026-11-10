#ifndef LOCKSTEP_GAME_NPCS_NPC_POSITIONS_HPP
#define LOCKSTEP_GAME_NPCS_NPC_POSITIONS_HPP

#include "Npcs/NpcContext.hpp"
#include "Npcs/NpcTactics.hpp"

#include <cstddef>
#include <cstdint>

namespace game
{
    // Height of a threat's eyes above where it was last seen or heard, metres.
    inline constexpr math::Vec3 k_npcThreatEyeHeight{0.0f, 1.5f, 0.0f};

    // An NPC this close to its cover spot is in it, metres.
    inline constexpr float k_npcInCoverDistance{0.6f};

    // foot:    where an NPC stands.
    // world:   the tick's NPC context.
    // Returns: the nearest player (downed ones last), or null when there is none.
    [[nodiscard]] const NpcActor* nearestPlayer(const math::Vec3& foot, const NpcWorld& world);

    // Host: makes an NPC hide at its cover spot (crouched behind low cover) for a random while.
    // npc:      the NPC.
    // random:   xorshift state.
    // settings: hiding times.
    void enterCover(Npc& npc, uint32_t& random, const NpcSettings& settings);

    // Host: what cover an NPC looks for against a threat: near itself, or for the squad near the
    // nearest player.
    // npc:       the NPC.
    // foot:      where it stands.
    // threatEye: the enemy's eyes.
    // world:     the tick's NPC context.
    // Returns:   the query (callers adjust it for flanks, advances and falling back).
    [[nodiscard]] CoverQuery coverQuery(const Npc& npc, const math::Vec3& foot, const math::Vec3& threatEye,
        const NpcWorld& world);

    // Host: finds cover (keeping clear of spots other NPCs hold or head for, and of the spot it
    // last got stuck on) and runs there, or hides at once when already there.
    // roster: the roster.
    // index:  the NPC; must be alive.
    // world:  the tick's NPC context.
    // foot:   where it stands.
    // reason: why it moves.
    // query:  what to look for.
    // Returns: true if it found cover and a path to it.
    bool seekCover(NpcRoster& roster, std::size_t index, const NpcWorld& world, const math::Vec3& foot,
        NpcMoveReason reason, const CoverQuery& query);
} //namespace game

#endif //LOCKSTEP_GAME_NPCS_NPC_POSITIONS_HPP
