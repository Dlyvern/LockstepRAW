#include "Npcs/NpcTactics.hpp"

#include "Npcs/NpcRoster.hpp"
#include "Npcs/NpcSenses.hpp"

#include <Lockstep/Physics/PhysicsWorld.hpp>

#include <algorithm>
#include <cmath>
#include <utility>
#include <vector>

namespace game
{
    namespace
    {
        namespace physics = lockstep::physics;

        constexpr math::Vec3 k_up{0.0f, 1.0f, 0.0f};
        constexpr float k_hiddenHeadHeight{1.0f};
        constexpr float k_standingHeadHeight{1.6f};
        constexpr float k_kneeHeight{0.5f};
        constexpr float k_protectionReach{2.0f};
        constexpr float k_sideStep{0.75f};
        constexpr float k_floorProbe{0.8f};
        constexpr float k_sameFloorHeight{1.0f};
        constexpr float k_takenSpacing{1.2f};
        constexpr float k_randomJitter{1.0f};
        constexpr std::size_t k_checkedCandidates{24};

        bool isHiddenAt(const physics::PhysicsWorld& world, const math::Vec3& foot, const math::Vec3& threat)
        {
            const math::Vec3 head = foot + k_up * k_hiddenHeadHeight;
            const math::Vec3 offset = threat - head;
            const float distance = math::length(offset);

            if (distance <= k_protectionReach)
            {
                return false;
            }

            const math::Vec3 nearPoint = head + offset * (k_protectionReach / distance);
            return isLevelBlocking(world, head, nearPoint);
        }

        bool seesFrom(const physics::PhysicsWorld& world, const math::Vec3& foot, const math::Vec3& threat)
        {
            return !isLevelBlocking(world, foot + k_up * k_standingHeadHeight, threat);
        }

        bool hasFloor(const physics::PhysicsWorld& world, const math::Vec3& foot)
        {
            const std::optional<physics::RaycastHit> hit = world.raycast(foot + k_up * 0.3f,
                math::Vec3{0.0f, -1.0f, 0.0f}, k_floorProbe);
            return hit.has_value();
        }

        std::optional<CoverSpot> evaluate(const physics::PhysicsWorld& world, const math::Vec3& node,
            const math::Vec3& threat, bool needsLineOfFire)
        {
            if (!isHiddenAt(world, node, threat))
            {
                return std::nullopt;
            }

            if (seesFrom(world, node, threat))
            {
                return CoverSpot{.hide = node, .peek = node, .isLow = true};
            }

            if (!needsLineOfFire)
            {
                return CoverSpot{.hide = node, .peek = node, .isLow = false};
            }

            const math::Vec3 toThreat{threat.x - node.x, 0.0f, threat.z - node.z};
            const float length = math::length(toThreat);

            if (length <= 0.0f)
            {
                return std::nullopt;
            }

            const math::Vec3 side{-toThreat.z / length, 0.0f, toThreat.x / length};

            for (const float sign : {1.0f, -1.0f})
            {
                const math::Vec3 peek = node + side * (sign * k_sideStep);

                if (!isLevelBlocking(world, node + k_up * k_kneeHeight, peek + k_up * k_kneeHeight)
                    && !isLevelBlocking(world, node + k_up * k_standingHeadHeight, peek + k_up * k_standingHeadHeight)
                    && hasFloor(world, peek) && seesFrom(world, peek, threat))
                {
                    return CoverSpot{.hide = node, .peek = peek, .isLow = false};
                }
            }

            return std::nullopt;
        }

        bool facesThreat(const NavNode& node, const math::Vec3& threat)
        {
            const float angle = std::atan2(threat.z - node.position.z, threat.x - node.position.x);
            const auto direction = static_cast<int32_t>(std::lround(angle / (0.25f * math::k_pi)));
            const auto bitOf = [](int32_t index) { return 1u << static_cast<uint32_t>((index % 8 + 8) % 8); };
            const uint32_t wanted = bitOf(direction - 1) | bitOf(direction) | bitOf(direction + 1);

            return (node.coverMask & wanted) != 0;
        }

        bool isTaken(const math::Vec3& position, std::span<const math::Vec3> taken)
        {
            return std::any_of(taken.begin(), taken.end(), [&position](const math::Vec3& other)
            {
                return math::length(other - position) < k_takenSpacing;
            });
        }
    } //namespace

    std::optional<CoverSpot> findCover(const NavGrid& grid, const physics::PhysicsWorld& world,
        const CoverQuery& query, uint32_t& random)
    {
        std::vector<std::pair<float, uint32_t>> ranked;
        const float radiusSquared = query.radius * query.radius;

        for (const uint32_t index : grid.cover)
        {
            const math::Vec3& position = grid.nodes[index].position;
            const math::Vec3 offset = position - query.from;

            if (std::abs(offset.y) > k_sameFloorHeight || offset.x * offset.x + offset.z * offset.z > radiusSquared)
            {
                continue;
            }

            const float threatDistance = math::length(query.threat - position);

            if (threatDistance < query.minThreatDistance || threatDistance > query.maxThreatDistance
                || !facesThreat(grid.nodes[index], query.threat) || isTaken(position, query.taken))
            {
                continue;
            }

            const float score = math::length(offset) + query.anchorWeight * math::length(query.anchor - position)
                + randomUnit(random) * k_randomJitter;
            ranked.emplace_back(score, index);
        }

        const std::size_t checked = std::min(ranked.size(), k_checkedCandidates);
        std::partial_sort(ranked.begin(), ranked.begin() + static_cast<std::ptrdiff_t>(checked), ranked.end());

        for (std::size_t candidate = 0; candidate < checked; ++candidate)
        {
            if (std::optional<CoverSpot> spot = evaluate(world, grid.nodes[ranked[candidate].second].position,
                    query.threat, query.needsLineOfFire))
            {
                return spot;
            }
        }

        return std::nullopt;
    }

    bool isCoverValid(const physics::PhysicsWorld& world, const CoverSpot& cover, const math::Vec3& threat)
    {
        return isHiddenAt(world, cover.hide, threat);
    }
} //namespace game
