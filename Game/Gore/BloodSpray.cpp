#include "Gore/BloodSpray.hpp"

#include <Lockstep/Core/Transform.hpp>
#include <Lockstep/Physics/PhysicsWorld.hpp>
#include <Lockstep/Render/Renderer.hpp>

#include <algorithm>
#include <cmath>

namespace game
{
    namespace
    {
        constexpr math::Vec3 k_gravity{0.0f, -9.81f, 0.0f};
        constexpr math::Vec3 k_lengthAxis{0.0f, 0.0f, 1.0f};
        constexpr float k_minSize{0.004f};
        constexpr float k_maxSize{0.014f};
        constexpr float k_lifeSeconds{2.5f};
        constexpr float k_drag{0.6f};
        constexpr float k_streak{0.025f};
        constexpr float k_minimumTravel{1.0e-5f};
        constexpr float k_centimetre{0.01f};
        constexpr float k_minDripSize{0.025f};

        math::Vec3 randomUnit(GoreRandom& random)
        {
            const float y = random.range(-1.0f, 1.0f);
            const float angle = random.range(0.0f, 2.0f * math::k_pi);
            const float ring = std::sqrt(std::max(0.0f, 1.0f - y * y));
            return math::Vec3{ring * std::cos(angle), y, ring * std::sin(angle)};
        }
    } //namespace

    void throwDroplets(BloodSpray& spray, const SprayBurst& burst, uint32_t maxDroplets, GoreRandom& random)
    {
        for (uint32_t index = 0; index < burst.count && maxDroplets > 0; ++index)
        {
            if (spray.droplets.size() >= maxDroplets)
            {
                spray.droplets.erase(spray.droplets.begin());
            }

            const math::Vec3 direction = math::normalize(burst.direction + randomUnit(random) * (burst.spread * 1.5f));
            spray.droplets.push_back(Droplet{.position = burst.point,
                .velocity = direction * (burst.speed * random.range(0.35f, 1.3f)),
                .size = random.range(k_minSize, k_maxSize),
                .seconds = k_lifeSeconds});
        }
    }

    void updateSpray(BloodSpray& spray, const lockstep::physics::PhysicsWorld& physics, const DripTarget& target,
        float seconds, GoreRandom& random)
    {
        if (seconds <= 0.0f)
        {
            return;
        }

        std::erase_if(spray.droplets, [&](Droplet& droplet)
        {
            droplet.seconds -= seconds;
            droplet.velocity += k_gravity * seconds;
            droplet.velocity *= std::max(0.0f, 1.0f - k_drag * seconds);
            const math::Vec3 travel = droplet.velocity * seconds;
            const float distance = math::length(travel);

            if (distance > k_minimumTravel)
            {
                const std::optional<lockstep::physics::RaycastHit> hit = physics.raycast(droplet.position,
                    travel / distance, distance);

                if (hit && isPaintable(target.decals, *hit))
                {
                    BloodDecal decal = decalOnSurface(hit->point, hit->normal, random.range(0.0f, 2.0f * math::k_pi));
                    decal.material = target.material;
                    decal.size = target.dripSize * (droplet.size / k_centimetre) * random.range(0.7f, 1.4f);
                    decal.stretch = 1.0f + std::min(math::length(droplet.velocity) * 0.08f, 0.8f);

                    if (fitDecal(physics, decal, k_minDripSize))
                    {
                        addDecal(target.decals, decal, target.maxDecals);
                    }

                    return true;
                }
            }

            droplet.position += travel;
            return droplet.seconds <= 0.0f;
        });
    }

    void drawSpray(lockstep::render::Renderer& renderer, const BloodSpray& spray, lockstep::render::MeshHandle mesh,
        lockstep::render::MaterialHandle material)
    {
        for (const Droplet& droplet : spray.droplets)
        {
            const float speed = math::length(droplet.velocity);
            const math::Quat rotation = speed > k_minimumTravel
                ? rotationBetween(k_lengthAxis, droplet.velocity / speed) : math::Quat{};
            renderer.submit(mesh, material, math::Transform{.position = droplet.position,
                .rotation = rotation,
                .scale = math::Vec3{droplet.size, droplet.size, droplet.size + speed * k_streak}});
        }
    }
} //namespace game
