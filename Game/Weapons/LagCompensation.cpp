#include "Weapons/LagCompensation.hpp"

#include <Lockstep/Physics/PhysicsWorld.hpp>

#include <algorithm>
#include <iterator>
#include <optional>
#include <utility>

namespace game
{
    namespace
    {
        namespace physics = lockstep::physics;

        std::optional<math::Vec3> poseOf(const LagFrame& frame, physics::ControllerId controller)
        {
            const auto pose = std::find_if(frame.poses.begin(), frame.poses.end(),
                [&](const RewindPose& held) { return held.controller == controller; });

            if (pose == frame.poses.end())
            {
                return std::nullopt;
            }

            return pose->foot;
        }
    } //namespace

    void recordLagFrame(LagHistory& history, const physics::PhysicsWorld& physics, uint64_t tick,
        std::span<const physics::ControllerId> controllers)
    {
        LagFrame frame{.tick = tick, .poses = {}};

        for (const physics::ControllerId controller : controllers)
        {
            if (const std::optional<math::Vec3> foot = physics.footPosition(controller))
            {
                frame.poses.push_back(RewindPose{.controller = controller, .foot = *foot});
            }
        }

        history.frames.push_back(std::move(frame));

        while (history.frames.size() > k_maxRewindTicks + 1)
        {
            history.frames.pop_front();
        }
    }

    Rewind rewindTo(const LagHistory& history, physics::PhysicsWorld& physics, double seenTick,
        physics::ControllerId shooter)
    {
        if (history.frames.empty())
        {
            return Rewind{};
        }

        const double newest = static_cast<double>(history.frames.back().tick);
        const double oldest = std::max(static_cast<double>(history.frames.front().tick),
            newest - static_cast<double>(k_maxRewindTicks));
        const double target = std::clamp(seenTick, oldest, newest);

        const auto after = std::find_if(history.frames.begin(), history.frames.end(),
            [&](const LagFrame& frame) { return static_cast<double>(frame.tick) > target; });
        const LagFrame& before = after == history.frames.begin() ? *after : *std::prev(after);
        const LagFrame& next = after == history.frames.end() ? before : *after;
        const double span = static_cast<double>(next.tick) - static_cast<double>(before.tick);
        const float blend = span > 0.0 ? static_cast<float>((target - static_cast<double>(before.tick)) / span) : 0.0f;

        Rewind rewind{.restore = {}, .ticks = newest - target};

        for (const RewindPose& pose : before.poses)
        {
            const std::optional<math::Vec3> current = physics.footPosition(pose.controller);

            if (pose.controller == shooter || !current)
            {
                continue;
            }

            const math::Vec3 then = math::lerp(pose.foot, poseOf(next, pose.controller).value_or(pose.foot), blend);
            rewind.restore.push_back(RewindPose{.controller = pose.controller, .foot = *current});
            physics.setFootPosition(pose.controller, then);
        }

        return rewind;
    }

    void restoreRewind(physics::PhysicsWorld& physics, const Rewind& rewind)
    {
        for (const RewindPose& pose : rewind.restore)
        {
            physics.setFootPosition(pose.controller, pose.foot);
        }
    }
} //namespace game
