#include "Npcs/NpcMovement.hpp"

#include "Npcs/NpcRoster.hpp"
#include "Npcs/NpcSenses.hpp"
#include "Player/PawnMovement.hpp"

#include <Lockstep/Physics/PhysicsWorld.hpp>

#include <algorithm>
#include <cmath>

namespace game
{
    namespace
    {
        namespace physics = lockstep::physics;

        constexpr math::Vec3 k_up{0.0f, 1.0f, 0.0f};
        constexpr float k_arriveDistance{0.3f};
        constexpr float k_shortcutStep{0.3f};
        constexpr float k_kneeHeight{0.3f};
        constexpr float k_shortcutReach{8.0f};
        constexpr float k_chestHeight{1.2f};
        constexpr float k_floorProbe{0.9f};
        constexpr float k_gravity{18.0f};
        constexpr float k_groundStick{2.0f};
        constexpr float k_crouchSpeedScale{0.5f};
        constexpr float k_stuckProgress{0.25f};
        constexpr float k_stuckSeconds{1.0f};
        constexpr float k_faceThreatReach{25.0f};
        constexpr std::size_t k_maxPathNodes{400};
        constexpr float k_goalSnap{0.6f};

        float yawOf(const math::Vec3& direction)
        {
            return std::atan2(-direction.x, -direction.z);
        }

        bool canWalkStraight(const physics::PhysicsWorld& world, const math::Vec3& from, const math::Vec3& to)
        {
            const math::Vec3 flat{to.x - from.x, 0.0f, to.z - from.z};

            if (std::abs(to.y - from.y) > k_shortcutStep || math::length(flat) > k_shortcutReach)
            {
                return false;
            }

            const math::Vec3 middle = (from + to) * 0.5f;

            return !isLevelBlocking(world, from + k_up * k_kneeHeight, to + k_up * k_kneeHeight)
                && !isLevelBlocking(world, from + k_up * k_chestHeight, to + k_up * k_chestHeight)
                && world.raycast(middle + k_up * 0.3f, math::Vec3{0.0f, -1.0f, 0.0f}, k_floorProbe).has_value();
        }

        void matchHeight(Npc& npc, physics::PhysicsWorld& world)
        {
            const float wanted = npc.isCrouched ? k_npcCrouchHeight : k_pawnStandHeight;
            const std::optional<float> height = world.controllerHeight(npc.controller);

            if (height && std::abs(*height - wanted) > 0.01f
                && !world.setControllerHeight(npc.controller, wanted, physics::ControllerResize::MustFit))
            {
                npc.isCrouched = true;
            }
        }
    } //namespace

    bool startNpcMove(Npc& npc, const NpcWorld& world, const math::Vec3& goal, NpcMoveReason reason, bool isRunning)
    {
        const std::optional<math::Vec3> foot = world.physics.footPosition(npc.controller);

        if (!foot)
        {
            return false;
        }

        std::optional<std::vector<math::Vec3>> path = findNavPath(world.grid, *foot, goal);

        if (!path || path->empty() || path->size() > k_maxPathNodes)
        {
            return false;
        }

        if (math::length(path->back() - goal) < k_goalSnap)
        {
            path->back() = goal;
        }

        npc.path = std::move(*path);
        npc.pathIndex = 0;
        npc.moveReason = reason;
        npc.isRunning = isRunning;
        npc.stuckSeconds = 0.0f;
        npc.action = NpcAction::Move;
        npc.isCrouched = false;

        return true;
    }

    void stopNpc(Npc& npc)
    {
        npc.path.clear();
        npc.pathIndex = 0;
        npc.moveReason = NpcMoveReason::None;
    }

    NpcMoveResult stepNpcMovement(Npc& npc, const NpcWorld& world)
    {
        const std::optional<math::Vec3> foot = world.physics.footPosition(npc.controller);

        if (!foot || world.delta <= 0.0f)
        {
            return npc.path.empty() ? NpcMoveResult::Idle : NpcMoveResult::Moving;
        }

        matchHeight(npc, world.physics);
        NpcMoveResult result = NpcMoveResult::Idle;
        math::Vec3 displacement{0.0f, 0.0f, 0.0f};
        float expected = 0.0f;

        if (npc.pathIndex < npc.path.size() && npc.pathIndex + 1 < npc.path.size()
            && canWalkStraight(world.physics, *foot, npc.path[npc.pathIndex + 1]))
        {
            ++npc.pathIndex;
        }

        while (npc.pathIndex < npc.path.size())
        {
            const math::Vec3 waypoint = npc.path[npc.pathIndex];
            const math::Vec3 flat{waypoint.x - foot->x, 0.0f, waypoint.z - foot->z};
            const float distance = math::length(flat);

            if (distance < k_arriveDistance)
            {
                ++npc.pathIndex;
                continue;
            }

            const float speed = (npc.isRunning ? world.settings.runSpeed : world.settings.walkSpeed)
                * (npc.isCrouched ? k_crouchSpeedScale : 1.0f);
            expected = std::min(speed * world.delta, distance);
            const math::Vec3 direction = flat * (1.0f / distance);
            displacement = direction * expected;
            result = NpcMoveResult::Moving;
            const bool facesThreat = npc.mode == NpcMode::Combat && npc.hasThreat
                && math::length(npc.threat - *foot) < k_faceThreatReach && !npc.isRunning;

            if (facesThreat)
            {
                turnNpcToward(npc, *foot, npc.threat, world.settings, world.delta);
            }
            else
            {
                turnNpcToYaw(npc, yawOf(direction), world.settings, world.delta);
            }

            break;
        }

        if (result == NpcMoveResult::Idle && !npc.path.empty())
        {
            stopNpc(npc);
            result = NpcMoveResult::Arrived;
        }

        npc.verticalSpeed = npc.isGrounded ? -k_groundStick : npc.verticalSpeed - k_gravity * world.delta;
        displacement.y += npc.verticalSpeed * world.delta;

        if (const std::optional<physics::ControllerMove> moved = world.physics.move(npc.controller, displacement,
                world.delta))
        {
            npc.isGrounded = moved->grounded;
        }

        if (result == NpcMoveResult::Moving)
        {
            const std::optional<math::Vec3> after = world.physics.footPosition(npc.controller);
            const float progress = after ? math::length(math::Vec3{after->x - foot->x, 0.0f, after->z - foot->z}) : 0.0f;
            npc.stuckSeconds = progress < expected * k_stuckProgress ? npc.stuckSeconds + world.delta : 0.0f;

            if (npc.stuckSeconds > k_stuckSeconds)
            {
                npc.avoidSpot = npc.path.back();
                stopNpc(npc);
                result = NpcMoveResult::Stuck;
            }
        }

        placeNpcEntity(npc, world.entities, world.physics, npc.isCrouched ? k_npcCrouchHeight : k_pawnStandHeight);

        return result;
    }

    float turnNpcToward(Npc& npc, const math::Vec3& from, const math::Vec3& point, const NpcSettings& settings,
        float delta)
    {
        const math::Vec3 flat{point.x - from.x, 0.0f, point.z - from.z};

        if (math::length(flat) <= 0.0f)
        {
            return 0.0f;
        }

        return turnNpcToYaw(npc, yawOf(flat), settings, delta);
    }

    float turnNpcToYaw(Npc& npc, float yaw, const NpcSettings& settings, float delta)
    {
        const float difference = std::remainder(yaw - npc.yaw, 2.0f * math::k_pi);
        const float step = settings.turnRate * delta;
        npc.yaw = std::remainder(npc.yaw + math::clamp(difference, -step, step), 2.0f * math::k_pi);

        return std::max(std::abs(difference) - step, 0.0f);
    }
} //namespace game
