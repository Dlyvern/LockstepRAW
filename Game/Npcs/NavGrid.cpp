#include "Npcs/NavGrid.hpp"

#include <Lockstep/Physics/PhysicsWorld.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <queue>
#include <utility>

namespace game
{
    namespace
    {
        namespace physics = lockstep::physics;

        constexpr math::Vec3 k_up{0.0f, 1.0f, 0.0f};
        constexpr math::Vec3 k_down{0.0f, -1.0f, 0.0f};
        constexpr std::array<std::array<int32_t, 2>, 8> k_directions{{
            {1, 0}, {1, 1}, {0, 1}, {-1, 1}, {-1, 0}, {-1, -1}, {0, -1}, {1, -1}}};
        constexpr int32_t k_maxFloorsPerColumn{8};
        constexpr int32_t k_maxSkippedControllers{4};
        constexpr float k_surfaceGap{0.05f};
        constexpr float k_kneeHeight{0.4f};
        constexpr float k_linkRayHeight{0.45f};
        constexpr float k_controllerSkip{0.7f};
        constexpr float k_nearestAbove{0.6f};
        constexpr float k_nearestBelow{1.6f};
        constexpr int32_t k_nearestReach{4};
        constexpr float k_heightWeight{4.0f};

        std::optional<physics::RaycastHit> staticRay(const physics::PhysicsWorld& world, math::Vec3 origin,
            const math::Vec3& direction, float distance)
        {
            for (int32_t attempt = 0; attempt <= k_maxSkippedControllers && distance > 0.0f; ++attempt)
            {
                const std::optional<physics::RaycastHit> hit = world.raycast(origin, direction, distance);

                if (!hit || hit->controller.isNull())
                {
                    return hit;
                }

                const float skipped = hit->distance + k_controllerSkip;
                origin = origin + direction * skipped;
                distance -= skipped;
            }

            return std::nullopt;
        }

        math::Vec3 directionOf(std::size_t direction)
        {
            const auto& step = k_directions[direction];
            return math::normalize(math::Vec3{static_cast<float>(step[0]), 0.0f, static_cast<float>(step[1])});
        }

        bool hasRoom(const physics::PhysicsWorld& world, const math::Vec3& foot, const NavGridSettings& settings)
        {
            if (staticRay(world, foot + k_up * k_surfaceGap, k_up, settings.agentHeight))
            {
                return false;
            }

            const math::Vec3 knee = foot + k_up * k_kneeHeight;

            for (std::size_t direction = 0; direction < k_directions.size(); ++direction)
            {
                if (staticRay(world, knee, directionOf(direction), settings.agentRadius))
                {
                    return false;
                }
            }

            return true;
        }

        uint8_t coverMaskAt(const physics::PhysicsWorld& world, const math::Vec3& foot, const NavGridSettings& settings)
        {
            uint8_t mask = 0;
            const math::Vec3 chest = foot + k_up * settings.coverHeight;

            for (std::size_t direction = 0; direction < k_directions.size(); ++direction)
            {
                if (staticRay(world, chest, directionOf(direction), settings.coverReach))
                {
                    mask = static_cast<uint8_t>(mask | (1u << direction));
                }
            }

            return mask;
        }

        void sampleColumn(const physics::PhysicsWorld& world, NavGrid& grid, float x, float z, float top, float bottom)
        {
            const NavGridSettings& settings = grid.settings;

            for (int32_t floor = 0; floor < k_maxFloorsPerColumn && top > bottom; ++floor)
            {
                const std::optional<physics::RaycastHit> hit = staticRay(world, math::Vec3{x, top, z}, k_down,
                    top - bottom);

                if (!hit)
                {
                    return;
                }

                if (hit->normal.y >= settings.maxSlopeCos && hasRoom(world, hit->point, settings))
                {
                    NavNode node{.position = hit->point};
                    node.coverMask = coverMaskAt(world, hit->point, settings);
                    grid.nodes.push_back(node);
                }

                top = hit->point.y - k_surfaceGap;
            }
        }

        bool canWalkBetween(const physics::PhysicsWorld& world, const NavNode& from, const NavNode& to)
        {
            const float base = std::max(from.position.y, to.position.y) + k_linkRayHeight;
            const math::Vec3 start{from.position.x, base, from.position.z};
            const math::Vec3 end{to.position.x, base, to.position.z};
            const math::Vec3 offset = end - start;
            const float distance = math::length(offset);

            return distance <= 0.0f || !staticRay(world, start, offset * (1.0f / distance), distance);
        }

        void linkNodes(const physics::PhysicsWorld& world, NavGrid& grid)
        {
            const auto columnsX = static_cast<int32_t>(grid.columns[0]);
            const auto columnsZ = static_cast<int32_t>(grid.columns[1]);

            for (int32_t cz = 0; cz < columnsZ; ++cz)
            {
                for (int32_t cx = 0; cx < columnsX; ++cx)
                {
                    const auto column = static_cast<std::size_t>(cx + cz * columnsX);

                    for (uint32_t index = grid.first[column]; index < grid.first[column + 1]; ++index)
                    {
                        for (std::size_t direction = 0; direction < k_directions.size(); ++direction)
                        {
                            const int32_t nx = cx + k_directions[direction][0];
                            const int32_t nz = cz + k_directions[direction][1];

                            if (nx < 0 || nz < 0 || nx >= columnsX || nz >= columnsZ)
                            {
                                continue;
                            }

                            const auto neighbour = static_cast<std::size_t>(nx + nz * columnsX);
                            NavNode& node = grid.nodes[index];
                            float bestStep = grid.settings.maxStep;

                            for (uint32_t other = grid.first[neighbour]; other < grid.first[neighbour + 1]; ++other)
                            {
                                const float step = std::abs(grid.nodes[other].position.y - node.position.y);

                                if (step <= bestStep && canWalkBetween(world, node, grid.nodes[other]))
                                {
                                    bestStep = step;
                                    node.links[direction] = static_cast<int32_t>(other);
                                }
                            }
                        }
                    }
                }
            }
        }

        struct OpenEntry
        {
            float score{0.0f};
            uint32_t node{0};

            friend bool operator>(const OpenEntry& left, const OpenEntry& right)
            {
                return left.score > right.score;
            }
        };
    } //namespace

    NavGrid buildNavGrid(const physics::PhysicsWorld& world, const math::Vec3& minimum, const math::Vec3& maximum,
        const NavGridSettings& settings)
    {
        NavGrid grid{.settings = settings};
        const float cell = std::max(settings.cellSize, 0.1f);
        auto columnsX = static_cast<uint32_t>(std::max((maximum.x - minimum.x) / cell, 1.0f));
        auto columnsZ = static_cast<uint32_t>(std::max((maximum.z - minimum.z) / cell, 1.0f));

        while (columnsX * columnsZ > settings.maxColumns)
        {
            columnsX = std::max(columnsX * 9 / 10, 1u);
            columnsZ = std::max(columnsZ * 9 / 10, 1u);
        }

        const math::Vec3 centre = (minimum + maximum) * 0.5f;
        grid.origin = math::Vec3{centre.x - 0.5f * cell * static_cast<float>(columnsX - 1), 0.0f,
            centre.z - 0.5f * cell * static_cast<float>(columnsZ - 1)};
        grid.columns = {columnsX, columnsZ};
        grid.first.reserve(static_cast<std::size_t>(columnsX) * columnsZ + 1);

        for (uint32_t cz = 0; cz < columnsZ; ++cz)
        {
            for (uint32_t cx = 0; cx < columnsX; ++cx)
            {
                grid.first.push_back(static_cast<uint32_t>(grid.nodes.size()));
                sampleColumn(world, grid, grid.origin.x + cell * static_cast<float>(cx),
                    grid.origin.z + cell * static_cast<float>(cz), maximum.y, minimum.y);
            }
        }

        grid.first.push_back(static_cast<uint32_t>(grid.nodes.size()));
        linkNodes(world, grid);

        for (uint32_t index = 0; index < grid.nodes.size(); ++index)
        {
            if (grid.nodes[index].coverMask != 0)
            {
                grid.cover.push_back(index);
            }
        }

        return grid;
    }

    std::optional<uint32_t> nearestNavNode(const NavGrid& grid, const math::Vec3& point)
    {
        if (grid.nodes.empty())
        {
            return std::nullopt;
        }

        const float cell = std::max(grid.settings.cellSize, 0.1f);
        const auto cx = static_cast<int32_t>(std::lround((point.x - grid.origin.x) / cell));
        const auto cz = static_cast<int32_t>(std::lround((point.z - grid.origin.z) / cell));
        const auto columnsX = static_cast<int32_t>(grid.columns[0]);
        const auto columnsZ = static_cast<int32_t>(grid.columns[1]);
        std::optional<uint32_t> best;
        float bestScore = std::numeric_limits<float>::max();

        for (int32_t z = cz - k_nearestReach; z <= cz + k_nearestReach; ++z)
        {
            for (int32_t x = cx - k_nearestReach; x <= cx + k_nearestReach; ++x)
            {
                if (x < 0 || z < 0 || x >= columnsX || z >= columnsZ)
                {
                    continue;
                }

                const auto column = static_cast<std::size_t>(x + z * columnsX);

                for (uint32_t index = grid.first[column]; index < grid.first[column + 1]; ++index)
                {
                    const math::Vec3& position = grid.nodes[index].position;
                    const float dy = position.y - point.y;

                    if (dy > k_nearestAbove || dy < -k_nearestBelow)
                    {
                        continue;
                    }

                    const float dx = position.x - point.x;
                    const float dz = position.z - point.z;
                    const float score = dx * dx + dz * dz + k_heightWeight * dy * dy;

                    if (score < bestScore)
                    {
                        bestScore = score;
                        best = index;
                    }
                }
            }
        }

        return best;
    }

    std::optional<std::vector<math::Vec3>> findNavPath(const NavGrid& grid, const math::Vec3& from,
        const math::Vec3& to, uint32_t maxExpansions)
    {
        const std::optional<uint32_t> start = nearestNavNode(grid, from);
        const std::optional<uint32_t> goal = nearestNavNode(grid, to);

        if (!start || !goal)
        {
            return std::nullopt;
        }

        const std::size_t count = grid.nodes.size();
        constexpr uint32_t k_none = std::numeric_limits<uint32_t>::max();
        std::vector<float> cost(count, std::numeric_limits<float>::max());
        std::vector<uint32_t> cameFrom(count, k_none);
        std::priority_queue<OpenEntry, std::vector<OpenEntry>, std::greater<>> open;
        const math::Vec3& goalPosition = grid.nodes[*goal].position;
        cost[*start] = 0.0f;
        open.push(OpenEntry{.score = math::length(goalPosition - grid.nodes[*start].position), .node = *start});
        uint32_t expansions = 0;
        bool isFound = false;

        while (!open.empty() && expansions < maxExpansions)
        {
            const OpenEntry current = open.top();
            open.pop();

            if (current.node == *goal)
            {
                isFound = true;
                break;
            }

            ++expansions;
            const NavNode& node = grid.nodes[current.node];

            for (const int32_t link : node.links)
            {
                if (link == k_noNavLink)
                {
                    continue;
                }

                const auto next = static_cast<uint32_t>(link);
                const float stepCost = cost[current.node] + math::length(grid.nodes[next].position - node.position);

                if (stepCost >= cost[next])
                {
                    continue;
                }

                cost[next] = stepCost;
                cameFrom[next] = current.node;
                open.push(OpenEntry{.score = stepCost + math::length(goalPosition - grid.nodes[next].position),
                    .node = next});
            }
        }

        if (!isFound)
        {
            return std::nullopt;
        }

        std::vector<math::Vec3> path;

        for (uint32_t node = *goal; node != k_none; node = cameFrom[node])
        {
            path.push_back(grid.nodes[node].position);

            if (node == *start)
            {
                break;
            }
        }

        std::reverse(path.begin(), path.end());
        return path;
    }
} //namespace game
