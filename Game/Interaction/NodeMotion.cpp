#include "Interaction/NodeMotion.hpp"

#include <Lockstep/Core/Bounds.hpp>
#include <Lockstep/Physics/PhysicsWorld.hpp>

#include <array>
#include <cmath>

namespace game
{
    namespace
    {
        namespace assets = lockstep::assets;

        constexpr math::Vec3 k_worldUp{0.0f, 1.0f, 0.0f};
        constexpr float k_probeAngle{0.785398f};

        math::Vec3 horizontal(const math::Vec3& vector)
        {
            return math::Vec3{vector.x, 0.0f, vector.z};
        }

        math::Aabb boundsInFrame(const assets::Scene& scene, std::span<const uint32_t> instances, const math::Mat4& toFrame)
        {
            math::Aabb bounds{};

            for (const uint32_t instance : instances)
            {
                const assets::SceneInstance& placed = scene.instances[instance];
                const math::Aabb& local = scene.meshes[placed.mesh].localBounds;
                const math::Mat4 toFrameFromMesh = toFrame * math::toMatrix(placed.transform);

                for (int corner = 0; corner < 8; ++corner)
                {
                    const math::Vec3 point{(corner & 1) != 0 ? local.max.x : local.min.x,
                        (corner & 2) != 0 ? local.max.y : local.min.y, (corner & 4) != 0 ? local.max.z : local.min.z};
                    math::expand(bounds, math::transformPoint(toFrameFromMesh, point));
                }
            }

            return bounds;
        }

        float component(const math::Vec3& vector, int axis)
        {
            return axis == 0 ? vector.x : axis == 1 ? vector.y : vector.z;
        }

        void setComponent(math::Vec3& vector, int axis, float value)
        {
            (axis == 0 ? vector.x : axis == 1 ? vector.y : vector.z) = value;
        }

        math::Vec3 rotateAround(const Hinge& hinge, float angle, const math::Vec3& point)
        {
            return hinge.pivot + math::angleAxis(angle, hinge.axis) * (point - hinge.pivot);
        }
    } //namespace

    NodeMotion captureNodeMotion(const assets::Scene& scene, std::span<const uint32_t> instances)
    {
        NodeMotion motion{};
        motion.instances.assign(instances.begin(), instances.end());

        for (const uint32_t instance : instances)
        {
            motion.rest.push_back(scene.instances[instance].transform);
        }

        return motion;
    }

    void rotateNodeMotion(NodeMotion& motion, SceneLevel& level, lockstep::physics::PhysicsWorld& physics,
        const Hinge& hinge, float angle)
    {
        if (angle == motion.angle)
        {
            return;
        }

        const math::Quat rotation = math::angleAxis(angle, hinge.axis);

        for (std::size_t index = 0; index < motion.instances.size(); ++index)
        {
            const math::Transform& rest = motion.rest[index];
            const math::Transform moved{.position = hinge.pivot + rotation * (rest.position - hinge.pivot),
                .rotation = math::normalize(rotation * rest.rotation),
                .scale = rest.scale};
            assets::setInstanceTransform(level.scene, level.collision, physics, motion.instances[index], moved);
        }

        motion.angle = angle;
    }

    Hinge hingeAtNode(const assets::Scene& scene, uint32_t node)
    {
        const math::Transform& transform = scene.nodes[node].transform;

        return Hinge{.pivot = transform.position, .axis = math::normalize(transform.rotation * k_worldUp)};
    }

    Hinge hingeFromBounds(const assets::Scene& scene, uint32_t node, std::span<const uint32_t> instances)
    {
        const math::Transform& nodeTransform = scene.nodes[node].transform;
        const math::Transform frame{.position = nodeTransform.position, .rotation = nodeTransform.rotation};
        const math::Mat4 toWorld = math::toMatrix(frame);
        const math::Aabb local = boundsInFrame(scene, instances, math::inverse(toWorld));

        const std::array<math::Vec3, 3> axes{nodeTransform.rotation * math::Vec3{1.0f, 0.0f, 0.0f},
            nodeTransform.rotation * math::Vec3{0.0f, 1.0f, 0.0f}, nodeTransform.rotation * math::Vec3{0.0f, 0.0f, 1.0f}};
        int vertical = 0;

        for (int axis = 1; axis < 3; ++axis)
        {
            if (std::abs(math::dot(axes[axis], k_worldUp)) > std::abs(math::dot(axes[vertical], k_worldUp)))
            {
                vertical = axis;
            }
        }

        const int first = (vertical + 1) % 3;
        const int second = (vertical + 2) % 3;
        const math::Vec3 extent = math::size(local);
        const int width = component(extent, first) >= component(extent, second) ? first : second;
        const int thickness = width == first ? second : first;

        const float nearEdge = std::abs(component(local.min, width)) <= std::abs(component(local.max, width))
            ? component(local.min, width) : component(local.max, width);
        math::Vec3 pivot = math::center(local);
        setComponent(pivot, width, nearEdge);
        setComponent(pivot, thickness, 0.5f * (component(local.min, thickness) + component(local.max, thickness)));

        return Hinge{.pivot = math::transformPoint(toWorld, pivot), .axis = math::normalize(axes[vertical])};
    }

    void orientHinge(Hinge& hinge, const assets::Scene& scene, std::span<const uint32_t> instances,
        const math::Vec3& awayFrom)
    {
        math::Aabb bounds{};

        for (const uint32_t instance : instances)
        {
            math::expand(bounds, scene.instances[instance].worldBounds);
        }

        const math::Vec3 centre = math::center(bounds);
        const float opened = math::length(horizontal(rotateAround(hinge, k_probeAngle, centre) - awayFrom));
        const float reversed = math::length(horizontal(rotateAround(hinge, -k_probeAngle, centre) - awayFrom));

        if (reversed > opened)
        {
            hinge.axis = -hinge.axis;
        }
    }

    void setInstancesVisible(SceneLevel& level, lockstep::physics::PhysicsWorld& physics,
        std::span<const uint32_t> instances, bool isVisible)
    {
        for (const uint32_t instance : instances)
        {
            level.scene.instances[instance].isVisible = isVisible;

            if (!isVisible)
            {
                assets::removeInstanceCollider(level.collision, physics, instance);
            }
        }
    }
} //namespace game
