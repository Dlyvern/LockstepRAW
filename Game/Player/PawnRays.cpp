#include "Player/PawnRays.hpp"

#include <Lockstep/Physics/PhysicsWorld.hpp>

#include <algorithm>

namespace game
{
    namespace
    {
        namespace physics = lockstep::physics;

        constexpr int k_maxPassThroughs{4};
        constexpr float k_passThroughStep{0.01f};
    } //namespace

    std::optional<physics::RaycastHit> sharedRaycast(const physics::PhysicsWorld& physics, const math::Vec3& origin,
        const math::Vec3& direction, float distance, physics::ControllerId self, std::span<const physics::BodyId> ignored)
    {
        math::Vec3 start = origin;
        float travelled = 0.0f;

        for (int attempt = 0; attempt <= k_maxPassThroughs && travelled < distance; ++attempt)
        {
            std::optional<physics::RaycastHit> hit = physics.raycast(start, direction, distance - travelled,
                physics::RaycastIgnore{.controller = self});

            if (!hit)
            {
                return std::nullopt;
            }

            const bool isIgnored = !hit->controller.isNull()
                || std::find(ignored.begin(), ignored.end(), hit->body) != ignored.end();

            if (!isIgnored)
            {
                hit->distance += travelled;
                return hit;
            }

            const float advance = hit->distance + k_passThroughStep;
            travelled += advance;
            start = start + direction * advance;
        }

        return std::nullopt;
    }
} //namespace game
