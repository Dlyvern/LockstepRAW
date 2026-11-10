#ifndef LOCKSTEP_GAME_NPCS_NAV_GRID_HPP
#define LOCKSTEP_GAME_NPCS_NAV_GRID_HPP

#include <Lockstep/Core/Math.hpp>

#include <array>
#include <cstdint>
#include <optional>
#include <vector>

namespace lockstep::physics
{
    class PhysicsWorld;
} //namespace lockstep::physics

namespace game
{
    namespace math = lockstep::math;

    // Marks a missing link of a NavNode.
    inline constexpr int32_t k_noNavLink{-1};

    // One walkable spot of the navigation grid: a floor point with headroom and room for an
    // NPC's capsule.
    // position:  the floor point (feet).
    // links:     neighbour node per grid direction (E, NE, N, NW, W, SW, S, SE), or k_noNavLink.
    // coverMask: per grid direction, a bit set when a wall or prop stands within 1 m at chest
    //            height of a crouched NPC (a candidate cover spot; checked against the threat at
    //            run time).
    struct NavNode
    {
        math::Vec3 position{0.0f, 0.0f, 0.0f};
        std::array<int32_t, 8> links{k_noNavLink, k_noNavLink, k_noNavLink, k_noNavLink, k_noNavLink, k_noNavLink,
            k_noNavLink, k_noNavLink};
        uint8_t coverMask{0};
    };

    // Settings for buildNavGrid().
    // cellSize:     grid spacing, metres.
    // agentRadius:  room needed around a node at knee height, metres.
    // agentHeight:  headroom needed above a node, metres.
    // maxStep:      largest height difference between linked neighbours (stairs), metres.
    // maxSlopeCos:  smallest floor normal y that counts as walkable.
    // coverReach:   a wall this close (at coverHeight) makes a node a cover candidate, metres.
    // coverHeight:  height above the floor of the cover rays, metres.
    // maxColumns:   cap on columns probed (the region is clipped around its centre beyond it).
    struct NavGridSettings
    {
        float cellSize{0.5f};
        float agentRadius{0.25f};
        float agentHeight{1.75f};
        float maxStep{0.45f};
        float maxSlopeCos{0.7f};
        float coverReach{1.0f};
        float coverHeight{0.9f};
        uint32_t maxColumns{160000};
    };

    // A grid of walkable floor points sampled from the level's colliders with raycasts when the
    // level loads (host only), with 8-way links between neighbours a capsule can walk between and
    // cover candidates. Several floors stack in one column. Limits: static geometry as it was at
    // load (no doors or moved props), no jumps, drops or mantles, ladders or lifts; agents are one
    // size (pawn capsule); narrow gaps under the cell size may close.
    // origin:  world x/z of column (0, 0)'s centre; y is unused.
    // columns: grid size in x and z.
    // first:   per column (x + z * columns.x), the index of its first node; nodes of a column are
    //          contiguous and `first` has one extra entry at the end.
    // nodes:   every node.
    // cover:   indices of nodes with a non-zero coverMask.
    // settings: the settings it was built with.
    struct NavGrid
    {
        math::Vec3 origin{0.0f, 0.0f, 0.0f};
        std::array<uint32_t, 2> columns{0, 0};
        std::vector<uint32_t> first{};
        std::vector<NavNode> nodes{};
        std::vector<uint32_t> cover{};
        NavGridSettings settings{};
    };

    // Samples the walkable floors inside a box.
    // physics:  the level's colliders (raycasts; controllers are skipped).
    // minimum:  the box's lowest corner.
    // maximum:  the box's highest corner.
    // settings: grid tuning.
    // Returns:  the grid (empty if nothing walkable was found).
    [[nodiscard]] NavGrid buildNavGrid(const lockstep::physics::PhysicsWorld& physics, const math::Vec3& minimum,
        const math::Vec3& maximum, const NavGridSettings& settings = {});

    // Finds the node nearest to a point on a floor.
    // grid:    the grid.
    // point:   a foot position.
    // Returns: the node index, or nothing when no node lies within ~2 m.
    [[nodiscard]] std::optional<uint32_t> nearestNavNode(const NavGrid& grid, const math::Vec3& point);

    // Finds a path with A* over the links.
    // grid:          the grid.
    // from, to:      foot positions.
    // maxExpansions: nodes searched before giving up.
    // Returns:       the foot positions to walk through (ending at `to`'s node), or nothing when
    //                either end is off the grid or no path was found.
    [[nodiscard]] std::optional<std::vector<math::Vec3>> findNavPath(const NavGrid& grid, const math::Vec3& from,
        const math::Vec3& to, uint32_t maxExpansions = 8000);
} //namespace game

#endif //LOCKSTEP_GAME_NPCS_NAV_GRID_HPP
