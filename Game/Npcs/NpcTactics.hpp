#ifndef LOCKSTEP_GAME_NPCS_NPC_TACTICS_HPP
#define LOCKSTEP_GAME_NPCS_NPC_TACTICS_HPP

#include "Npcs/NavGrid.hpp"
#include "Npcs/Npc.hpp"

#include <cstdint>
#include <optional>
#include <span>

namespace lockstep::physics
{
    class PhysicsWorld;
} //namespace lockstep::physics

namespace game
{
    // What kind of fighting position to look for.
    // from:              the NPC's feet.
    // threat:            the enemy's eyes (or where they were heard).
    // anchor:            a point the position should be near (the NPC itself, the players for the
    //                    squad, a flank side, a point away from the threat).
    // anchorWeight:      how much nearness to `anchor` counts against nearness to `from`.
    // minThreatDistance, maxThreatDistance: allowed distance to the threat, metres.
    // radius:            how far from `from` to look, metres.
    // taken:             positions other NPCs already hold or head for (kept 1.2 m apart).
    // needsLineOfFire:   the spot must let it shoot at the threat (standing up or stepping out);
    //                    false accepts any hidden spot facing it (waiting in ambush).
    struct CoverQuery
    {
        math::Vec3 from{0.0f, 0.0f, 0.0f};
        math::Vec3 threat{0.0f, 0.0f, 0.0f};
        math::Vec3 anchor{0.0f, 0.0f, 0.0f};
        float anchorWeight{0.0f};
        float minThreatDistance{3.0f};
        float maxThreatDistance{40.0f};
        float radius{12.0f};
        std::span<const math::Vec3> taken{};
        bool needsLineOfFire{true};
    };

    // Finds a fighting position among the grid's cover candidates near the NPC. The candidates are
    // ranked by distance (and the anchor), then the best few are checked with raycasts against the
    // threat: a crouched head hidden by level geometry close in front, and either a standing head
    // that sees the threat (low cover: stand up to shoot) or a spot 0.75 m to the side that sees
    // it (a corner or a door frame: step out to shoot).
    // grid:    the navigation grid.
    // physics: raycasts.
    // query:   what to look for.
    // random:  xorshift state, for a little variety between NPCs.
    // Returns: the position, or nothing when no candidate protects and lets it shoot.
    [[nodiscard]] std::optional<CoverSpot> findCover(const NavGrid& grid,
        const lockstep::physics::PhysicsWorld& physics, const CoverQuery& query, uint32_t& random);

    // Tells whether a cover spot still hides its holder from a threat (the threat may have
    // moved around it).
    // physics: raycasts.
    // cover:   the spot.
    // threat:  the enemy's eyes.
    // Returns: true while level geometry stands between the crouched head and the threat.
    [[nodiscard]] bool isCoverValid(const lockstep::physics::PhysicsWorld& physics, const CoverSpot& cover,
        const math::Vec3& threat);
} //namespace game

#endif //LOCKSTEP_GAME_NPCS_NPC_TACTICS_HPP
