#include "Weapons/WeaponDebris.hpp"

#include <Lockstep/Physics/PhysicsWorld.hpp>
#include <Lockstep/Render/Renderer.hpp>

#include <algorithm>
#include <cmath>
#include <optional>

namespace game
{
    namespace
    {
        namespace physics = lockstep::physics;

        constexpr math::Vec3 k_gravity{0.0f, -9.81f, 0.0f};
        constexpr math::Vec3 k_localX{1.0f, 0.0f, 0.0f};
        constexpr math::Vec3 k_localY{0.0f, 1.0f, 0.0f};
        constexpr math::Vec3 k_localZ{0.0f, 0.0f, 1.0f};
        constexpr float k_floorNormal{0.6f};
        constexpr float k_settleSeconds{0.12f};
        constexpr float k_shrinkSeconds{0.5f};
        constexpr float k_lowestHeight{-200.0f};
        constexpr float k_minStep{1e-5f};
        constexpr float k_maxSeconds{0.1f};

        math::Quat basisRotation(const math::Vec3& xAxis, const math::Vec3& yAxis)
        {
            const math::Vec3 z = math::cross(xAxis, yAxis);
            math::Mat4 basis{1.0f};
            basis.setColumn(0, math::Vec4{xAxis.x, xAxis.y, xAxis.z, 0.0f});
            basis.setColumn(1, math::Vec4{yAxis.x, yAxis.y, yAxis.z, 0.0f});
            basis.setColumn(2, math::Vec4{z.x, z.y, z.z, 0.0f});

            return math::fromRotationMatrix(basis);
        }

        math::Quat lyingRotation(const math::Quat& rotation, const math::Vec3& normal)
        {
            math::Vec3 along = rotation * k_localY;
            along = along - normal * math::dot(along, normal);

            if (math::length(along) < 0.1f)
            {
                along = rotation * k_localZ;
                along = along - normal * math::dot(along, normal);
            }

            if (math::length(along) < 1e-3f)
            {
                along = std::abs(normal.x) < 0.9f ? math::Vec3{1.0f, 0.0f, 0.0f} : math::Vec3{0.0f, 0.0f, 1.0f};
                along = along - normal * math::dot(along, normal);
            }

            const float side = math::dot(rotation * k_localX, normal) >= 0.0f ? 1.0f : -1.0f;

            return basisRotation(normal * side, math::normalize(along));
        }

        std::optional<physics::RaycastHit> castPast(const physics::PhysicsWorld& world, const DebrisPiece& piece,
            const math::Vec3& direction, float distance)
        {
            std::optional<physics::RaycastHit> hit = world.raycast(piece.position, direction, distance,
                physics::RaycastIgnore{.controller = piece.ignore});

            if (hit && !hit->controller.isNull())
            {
                hit = world.raycast(piece.position, direction, distance, physics::RaycastIgnore{.controller = hit->controller});
            }

            return hit && hit->controller.isNull() ? hit : std::nullopt;
        }

        void bounce(DebrisPool& pool, DebrisPiece& piece, const physics::RaycastHit& hit, const SceneLevel* level)
        {
            const DebrisKind& kind = pool.kind;
            const math::Vec3 normal = math::normalize(hit.normal);
            const float into = -math::dot(piece.velocity, normal);
            const math::Vec3 along = piece.velocity + normal * into;
            piece.position = hit.point + normal * kind.halfThickness;
            piece.velocity = along * kind.friction + normal * (std::max(into, 0.0f) * kind.restitution);
            const math::Vec3 kick = math::cross(normal, along);
            piece.spin = piece.spin * kind.spinKept
                + (math::length(kick) > 1e-4f ? math::normalize(kick) : k_localX) * (into * kind.spinKick);

            if (into > kind.soundSpeed && piece.bounces < kind.maxSoundBounces)
            {
                pool.landings.push_back(DebrisLanding{.position = hit.point,
                    .surface = surfaceOfBody(level, hit.body),
                    .loudness = std::clamp(into / kind.fullSoundSpeed, 0.15f, 1.0f)});
            }

            ++piece.bounces;

            if (normal.y > k_floorNormal && math::length(piece.velocity) < kind.restSpeed)
            {
                piece.isResting = true;
                piece.velocity = math::Vec3{0.0f, 0.0f, 0.0f};
                piece.spin = math::Vec3{0.0f, 0.0f, 0.0f};
                piece.rest = lyingRotation(piece.rotation, normal);
                piece.settle = 0.0f;
            }
        }

        void step(DebrisPool& pool, DebrisPiece& piece, const physics::PhysicsWorld& world, const SceneLevel* level,
            float seconds)
        {
            if (piece.isResting)
            {
                if (piece.settle < 1.0f)
                {
                    piece.settle = std::min(piece.settle + seconds / k_settleSeconds, 1.0f);
                    piece.rotation = math::slerp(piece.rotation, piece.rest, piece.settle);
                }

                return;
            }

            piece.velocity = piece.velocity + k_gravity * seconds;
            const math::Vec3 move = piece.velocity * seconds;
            const float distance = math::length(move);

            if (distance > k_minStep)
            {
                const math::Vec3 direction = move / distance;

                if (std::optional<physics::RaycastHit> hit = castPast(world, piece, direction,
                        distance + pool.kind.halfThickness))
                {
                    bounce(pool, piece, *hit, level);
                }
                else
                {
                    piece.position = piece.position + move;
                }
            }

            const float spin = math::length(piece.spin);

            if (spin > 1e-4f && !piece.isResting)
            {
                piece.rotation = math::normalize(math::angleAxis(spin * seconds, piece.spin / spin) * piece.rotation);
            }
        }
    } //namespace

    void spawnDebris(DebrisPool& pool, const DebrisPiece& piece)
    {
        if (pool.kind.maxPieces == 0)
        {
            return;
        }

        if (pool.pieces.size() < pool.kind.maxPieces)
        {
            pool.pieces.push_back(piece);
            return;
        }

        pool.next %= pool.pieces.size();
        pool.pieces[pool.next] = piece;
        pool.next = (pool.next + 1) % pool.pieces.size();
    }

    void updateDebris(DebrisPool& pool, const physics::PhysicsWorld& world, const SceneLevel* level, float seconds)
    {
        pool.landings.clear();
        const float clamped = std::clamp(seconds, 0.0f, k_maxSeconds);

        for (DebrisPiece& piece : pool.pieces)
        {
            if (piece.age < pool.kind.lifeSeconds)
            {
                piece.age += clamped;
                step(pool, piece, world, level, clamped);
            }
        }

        const auto expired = [&pool](const DebrisPiece& piece)
        {
            return piece.age >= pool.kind.lifeSeconds || piece.position.y < k_lowestHeight;
        };

        if (std::all_of(pool.pieces.begin(), pool.pieces.end(), expired))
        {
            pool.pieces.clear();
            pool.next = 0;
        }
    }

    void drawDebris(const DebrisPool& pool, lockstep::render::Renderer& renderer)
    {
        if (!pool.mesh.isValid() || !pool.material.isValid())
        {
            return;
        }

        for (const DebrisPiece& piece : pool.pieces)
        {
            const float left = pool.kind.lifeSeconds - piece.age;

            if (left <= 0.0f || piece.position.y < k_lowestHeight)
            {
                continue;
            }

            const float size = std::clamp(left / k_shrinkSeconds, 0.0f, 1.0f);
            renderer.submit(pool.mesh, pool.material,
                math::Transform{.position = piece.position, .rotation = piece.rotation, .scale = {size, size, size}});
        }
    }

    void clearDebris(DebrisPool& pool)
    {
        pool.pieces.clear();
        pool.landings.clear();
        pool.next = 0;
    }
} //namespace game
