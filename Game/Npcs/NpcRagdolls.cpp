#include "Npcs/NpcRagdolls.hpp"

#include <Lockstep/Physics/PhysicsWorld.hpp>

#include <algorithm>
#include <cmath>
#include <iostream>
#include <string_view>

#if LOCKSTEP_DEV_TOOLS
#include <imgui.h>
#endif

namespace game
{
    namespace
    {
        namespace anim = lockstep::anim;
        namespace physics = lockstep::physics;

        constexpr std::string_view k_chestBone{"Spine2|Spine1"};
        constexpr math::Vec3 k_up{0.0f, 1.0f, 0.0f};
        constexpr float k_parallelEpsilon{1.0e-8f};

        const physics::RagdollProfile& profileOf(NpcRagdolls& ragdolls)
        {
            if (!ragdolls.profile)
            {
                ragdolls.profile = physics::mixamoRagdollProfile();
                const auto& bones = ragdolls.profile->bones;
                const auto chest = std::find_if(bones.begin(), bones.end(),
                    [](const physics::RagdollBoneProfile& bone) { return bone.bone == k_chestBone; });
                ragdolls.chestBody = chest != bones.end() ? static_cast<uint32_t>(chest - bones.begin()) : 0u;
            }

            return *ragdolls.profile;
        }

        void reportFailure(NpcRagdolls& ragdolls, const lockstep::Error& error)
        {
            if (ragdolls.failures++ == 0)
            {
                std::clog << "Game: NPC ragdoll not created (" << error.message << "); bodies tip over instead\n";
            }
        }

        void fillDriven(NpcRagdoll& ragdoll)
        {
            ragdoll.driven.resize(ragdoll.poses.size());

            for (std::size_t index = 0; index < ragdoll.poses.size(); ++index)
            {
                ragdoll.driven[index] = anim::DrivenJoint{.joint = ragdoll.joints[index], .world = ragdoll.poses[index]};
            }
        }

        math::Vec3 intoFrame(const math::Transform& frame, const math::Vec3& point)
        {
            return math::conjugate(frame.rotation) * (point - frame.position);
        }

        math::Vec3 outOfFrame(const math::Transform& frame, const math::Vec3& point)
        {
            return frame.position + frame.rotation * point;
        }

        std::optional<physics::SkeletonRagdoll> describe(NpcRagdolls& ragdolls, const anim::SkinnedModel& model,
            const anim::SkinnedInstance& instance, const math::Transform& placement)
        {
            auto described = physics::describeSkeletonRagdoll(model.skeleton, profileOf(ragdolls),
                instance.jointMatrices(), placement);

            if (!described)
            {
                reportFailure(ragdolls, described.error());
                return std::nullopt;
            }

            described.value().description.settings = ragdolls.tuning.physics;
            return std::move(described).value();
        }

        std::optional<float> raySphere(const math::Vec3& origin, const math::Vec3& direction, const math::Vec3& centre,
            float radius)
        {
            const math::Vec3 offset = origin - centre;
            const float half = math::dot(offset, direction);
            const float rest = math::dot(offset, offset) - radius * radius;
            const float discriminant = half * half - rest;

            if (discriminant < 0.0f)
            {
                return std::nullopt;
            }

            const float near = -half - std::sqrt(discriminant);
            return near >= 0.0f ? std::optional<float>(near) : (rest <= 0.0f ? std::optional<float>(0.0f) : std::nullopt);
        }

        std::optional<float> rayCylinder(const math::Vec3& origin, const math::Vec3& direction, const math::Vec3& start,
            const math::Vec3& end, float radius)
        {
            const math::Vec3 axis = end - start;
            const float axisLength = math::dot(axis, axis);

            if (axisLength < k_parallelEpsilon)
            {
                return std::nullopt;
            }

            const math::Vec3 offset = origin - start;
            const math::Vec3 across = direction - axis * (math::dot(direction, axis) / axisLength);
            const math::Vec3 from = offset - axis * (math::dot(offset, axis) / axisLength);
            const float a = math::dot(across, across);

            if (a < k_parallelEpsilon)
            {
                return std::nullopt;
            }

            const float b = math::dot(across, from);
            const float c = math::dot(from, from) - radius * radius;
            const float discriminant = b * b - a * c;

            if (discriminant < 0.0f)
            {
                return std::nullopt;
            }

            const float distance = (-b - std::sqrt(discriminant)) / a;
            const float along = math::dot(offset + direction * distance, axis) / axisLength;

            if (distance < 0.0f || along < 0.0f || along > 1.0f)
            {
                return std::nullopt;
            }

            return distance;
        }

        std::optional<float> rayCapsule(const math::Vec3& origin, const math::Vec3& direction, const math::Vec3& start,
            const math::Vec3& end, float radius)
        {
            std::optional<float> nearest = rayCylinder(origin, direction, start, end, radius);

            for (const math::Vec3& cap : {start, end})
            {
                if (const std::optional<float> hit = raySphere(origin, direction, cap, radius))
                {
                    nearest = nearest ? std::min(*nearest, *hit) : *hit;
                }
            }

            return nearest;
        }

        void followAnchors(NpcRagdoll& ragdoll)
        {
            for (std::size_t body = 0; body < ragdoll.attachments.size() && body < ragdoll.poses.size(); ++body)
            {
                const RagdollAttachment& attachment = ragdoll.attachments[body];

                if (attachment.anchor < ragdoll.poses.size())
                {
                    const math::Transform& anchor = ragdoll.poses[attachment.anchor];
                    ragdoll.poses[body] = math::Transform{.position = outOfFrame(anchor, attachment.relative.position),
                        .rotation = anchor.rotation * attachment.relative.rotation};
                }
            }
        }

        bool isUnder(const anim::Skeleton& skeleton, uint32_t joint, uint32_t root)
        {
            for (uint32_t walk = joint; walk < skeleton.joints.size(); walk = skeleton.joints[walk].parent)
            {
                if (walk == root)
                {
                    return true;
                }
            }

            return false;
        }
    } //namespace

    std::optional<NpcRagdoll> startNpcRagdoll(physics::PhysicsWorld& physics, NpcRagdolls& ragdolls,
        const anim::SkinnedModel& model, const anim::SkinnedInstance& instance, const math::Transform& placement,
        const RagdollPush& push)
    {
        std::optional<physics::SkeletonRagdoll> described = describe(ragdolls, model, instance, placement);

        if (!described)
        {
            return std::nullopt;
        }

        auto created = physics.createRagdoll(described->description);

        if (!created)
        {
            reportFailure(ragdolls, created.error());
            return std::nullopt;
        }

        NpcRagdoll ragdoll{.id = created.value(),
            .joints = std::move(described->joints),
            .parents = {},
            .attachments = std::vector<RagdollAttachment>(described->description.bodies.size()),
            .capsules = {},
            .poses = {},
            .driven = {},
            .placement = placement,
            .order = ragdolls.nextOrder++};

        for (const physics::RagdollBodyDescription& body : described->description.bodies)
        {
            ragdoll.poses.push_back(body.pose);
            ragdoll.parents.push_back(body.parent);
            ragdoll.capsules.push_back(RagdollCapsule{.start = intoFrame(body.rest, body.capsuleStart),
                .end = intoFrame(body.rest, body.capsuleEnd),
                .radius = body.radius});
        }

        const math::Vec3 lifted = push.direction + k_up * ragdolls.tuning.shotLift;
        const math::Vec3 direction = math::lengthSquared(lifted) > k_parallelEpsilon ? math::normalize(lifted) : k_up;
        const uint32_t body = ragdollBodyOfLimb(ragdolls, model, ragdoll, push.limb);
        physics.applyRagdollImpulse(ragdoll.id, body, direction * push.impulse);
        fillDriven(ragdoll);
        return ragdoll;
    }

    bool wakeNpcRagdoll(physics::PhysicsWorld& physics, NpcRagdolls& ragdolls, const anim::SkinnedModel& model,
        const anim::SkinnedInstance& instance, NpcRagdoll& ragdoll)
    {
        if (isSimulating(ragdoll))
        {
            return true;
        }

        std::optional<physics::SkeletonRagdoll> described = describe(ragdolls, model, instance, ragdoll.placement);

        if (!described || described->joints != ragdoll.joints)
        {
            return false;
        }

        auto created = physics.createRagdoll(described->description);

        if (!created)
        {
            reportFailure(ragdolls, created.error());
            return false;
        }

        ragdoll.id = created.value();
        ragdoll.order = ragdolls.nextOrder++;
        ragdoll.awakeSeconds = 0.0f;
        ragdoll.isAsleep = false;
        return true;
    }

    std::span<const anim::DrivenJoint> updateNpcRagdoll(physics::PhysicsWorld& physics, NpcRagdoll& ragdoll,
        float alpha, float seconds)
    {
        ragdoll.seconds += seconds;

        if (!isSimulating(ragdoll))
        {
            return ragdoll.driven;
        }

        if (!physics.ragdollPoses(ragdoll.id, alpha, ragdoll.poses))
        {
            ragdoll.id = {};
            return ragdoll.driven;
        }

        followAnchors(ragdoll);
        fillDriven(ragdoll);
        ragdoll.awakeSeconds += seconds;
        const std::optional<physics::RagdollStatus> status = physics.ragdollStatus(ragdoll.id);
        ragdoll.isAsleep = status && status->isAsleep;
        return ragdoll.driven;
    }

    void freezeNpcRagdoll(physics::PhysicsWorld& physics, NpcRagdoll& ragdoll)
    {
        if (isSimulating(ragdoll))
        {
            physics.destroyRagdoll(ragdoll.id);
            ragdoll.id = {};
        }
    }

    void severNpcRagdollLimb(physics::PhysicsWorld& physics, const anim::SkinnedModel& model, NpcRagdoll& ragdoll,
        anim::Limb limb)
    {
        const anim::LimbInfo* info = anim::findLimb(model, limb);

        if (info == nullptr || ragdoll.attachments.size() != ragdoll.joints.size())
        {
            return;
        }

        std::optional<uint32_t> first;

        for (std::size_t body = 0; body < ragdoll.joints.size(); ++body)
        {
            if (!isUnder(model.skeleton, ragdoll.joints[body], info->rootJoint))
            {
                continue;
            }

            first = first ? std::min(*first, static_cast<uint32_t>(body)) : static_cast<uint32_t>(body);

            if (ragdoll.attachments[body].anchor != k_notAttached)
            {
                continue;
            }

            uint32_t anchor = ragdoll.parents[body];

            while (anchor < ragdoll.joints.size() && isUnder(model.skeleton, ragdoll.joints[anchor], info->rootJoint))
            {
                anchor = ragdoll.parents[anchor];
            }

            if (anchor >= ragdoll.poses.size())
            {
                continue;
            }

            const math::Transform& frame = ragdoll.poses[anchor];
            ragdoll.attachments[body] = RagdollAttachment{.anchor = anchor,
                .relative = math::Transform{.position = intoFrame(frame, ragdoll.poses[body].position),
                    .rotation = math::conjugate(frame.rotation) * ragdoll.poses[body].rotation}};
        }

        if (first && isSimulating(ragdoll))
        {
            physics.removeRagdollBodies(ragdoll.id, *first);
        }
    }

    bool isSimulating(const NpcRagdoll& ragdoll) noexcept
    {
        return !ragdoll.id.isNull();
    }

    std::optional<RagdollRayHit> traceNpcRagdoll(const NpcRagdoll& ragdoll, const math::Vec3& origin,
        const math::Vec3& direction, float maxDistance)
    {
        std::optional<RagdollRayHit> nearest;
        const std::size_t count = std::min(ragdoll.capsules.size(), ragdoll.poses.size());

        for (std::size_t body = 0; body < count; ++body)
        {
            if (body < ragdoll.attachments.size() && ragdoll.attachments[body].anchor != k_notAttached)
            {
                continue;
            }

            const RagdollCapsule& capsule = ragdoll.capsules[body];
            const math::Transform& pose = ragdoll.poses[body];
            const std::optional<float> distance = rayCapsule(origin, direction, outOfFrame(pose, capsule.start),
                outOfFrame(pose, capsule.end), capsule.radius);

            if (distance && *distance <= maxDistance && (!nearest || *distance < nearest->distance))
            {
                nearest = RagdollRayHit{.body = static_cast<uint32_t>(body),
                    .point = origin + direction * *distance,
                    .distance = *distance};
            }
        }

        return nearest;
    }

    math::Vec3 ragdollBodyCentre(const NpcRagdoll& ragdoll, uint32_t body)
    {
        if (body >= ragdoll.capsules.size() || body >= ragdoll.poses.size())
        {
            return ragdoll.placement.position;
        }

        const RagdollCapsule& capsule = ragdoll.capsules[body];
        return outOfFrame(ragdoll.poses[body], (capsule.start + capsule.end) * 0.5f);
    }

    uint32_t ragdollBodyOfLimb(const NpcRagdolls& ragdolls, const anim::SkinnedModel& model, const NpcRagdoll& ragdoll,
        anim::Limb limb)
    {
        const uint32_t last = ragdoll.joints.empty() ? 0u : static_cast<uint32_t>(ragdoll.joints.size() - 1);
        const anim::LimbInfo* info = limb == anim::Limb::Torso ? nullptr : anim::findLimb(model, limb);

        if (info == nullptr)
        {
            return std::min(ragdolls.chestBody, last);
        }

        const auto body = std::find_if(ragdoll.joints.begin(), ragdoll.joints.end(),
            [&](uint32_t joint) { return isUnder(model.skeleton, joint, info->rootJoint); });
        return body != ragdoll.joints.end() ? static_cast<uint32_t>(body - ragdoll.joints.begin())
                                            : std::min(ragdolls.chestBody, last);
    }

    void pushNpcRagdoll(physics::PhysicsWorld& physics, NpcRagdolls& ragdolls, const anim::SkinnedModel& model,
        const anim::SkinnedInstance& instance, NpcRagdoll& ragdoll, uint32_t body, const math::Vec3& impulse,
        const std::optional<math::Vec3>& point)
    {
        if (wakeNpcRagdoll(physics, ragdolls, model, instance, ragdoll))
        {
            physics.applyRagdollImpulse(ragdoll.id, body, impulse, point);
        }
    }

#if LOCKSTEP_DEV_TOOLS
    void npcRagdollsDevSection(NpcRagdolls& ragdolls, uint32_t simulating, uint32_t asleep, uint32_t frozen,
        const physics::PhysicsWorld& physics)
    {
        if (!ImGui::TreeNode("NPC ragdolls"))
        {
            return;
        }

        const physics::PhysicsStats stats = physics.stats();
        ImGui::Text("Simulating %u (%u asleep), frozen %u, bodies %u", simulating, asleep, frozen,
            stats.ragdollBodyCount);
        ImGui::Text("Physics step %.2f ms (whole scene)", static_cast<double>(stats.lastStepMilliseconds));

        RagdollTuning& tuning = ragdolls.tuning;
        int maxSimulating = static_cast<int>(tuning.maxSimulating);
        ImGui::Checkbox("Ragdolls", &tuning.isEnabled);

        if (ImGui::SliderInt("Max simulating", &maxSimulating, 1, 64))
        {
            tuning.maxSimulating = static_cast<uint32_t>(maxSimulating);
        }

        ImGui::SliderFloat("Min time before cap freeze (s)", &tuning.minSeconds, 0.0f, 5.0f);
        ImGui::SliderFloat("Killing shot impulse (kg m/s)", &tuning.shotImpulse, 0.0f, 400.0f);
        ImGui::SliderFloat("Bullet impulse on bodies (kg m/s)", &tuning.hitImpulse, 0.0f, 400.0f);
        ImGui::SliderFloat("Shot lift", &tuning.shotLift, 0.0f, 1.0f);
        ImGui::SliderFloat("Blend in (s)", &tuning.blendSeconds, 0.0f, 0.5f);
        ImGui::TextDisabled("Applies to new ragdolls:");
        physics::RagdollSettings& settings = tuning.physics;
        ImGui::SliderFloat("Joint damping", &settings.jointDamping, 0.0f, 20.0f);
        ImGui::SliderFloat("Angular damping", &settings.angularDamping, 0.0f, 5.0f);
        ImGui::SliderFloat("Linear damping", &settings.linearDamping, 0.0f, 2.0f);
        ImGui::SliderFloat("Friction", &settings.friction, 0.0f, 2.0f);
        ImGui::SliderFloat("Sleep threshold", &settings.sleepThreshold, 0.0f, 0.2f);
        ImGui::Checkbox("Self collision", &settings.selfCollision);
        ImGui::TreePop();
    }
#endif
} //namespace game
