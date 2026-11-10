#include "Lockstep/Anim/Limbs.hpp"

#include "Lockstep/Anim/SkinnedModel.hpp"

#include <limits>

namespace lockstep::anim
{
    namespace
    {
        constexpr std::array<Limb, k_limbCount> k_parents{Limb::None, Limb::Torso, Limb::Torso, Limb::UpperArmLeft,
            Limb::Torso, Limb::UpperArmRight, Limb::Torso, Limb::ThighLeft, Limb::Torso, Limb::ThighRight};
        constexpr std::array<const char*, k_limbCount> k_names{"torso", "head", "left upper arm", "left forearm",
            "right upper arm", "right forearm", "left thigh", "left shin", "right thigh", "right shin"};

        bool isLimb(Limb limb)
        {
            return static_cast<std::size_t>(limb) < k_limbCount;
        }

        uint64_t partBit(std::size_t index)
        {
            return index < std::numeric_limits<uint64_t>::digits ? uint64_t{1} << index : 0;
        }

        float segmentDistanceSquared(const math::Vec3& point, const math::Vec3& from, const math::Vec3& to)
        {
            const math::Vec3 segment = to - from;
            const float lengthSquared = math::dot(segment, segment);
            const float t = lengthSquared > 0.0f ? math::clamp(math::dot(point - from, segment) / lengthSquared, 0.0f, 1.0f)
                                                 : 0.0f;
            const math::Vec3 offset = point - (from + segment * t);

            return math::dot(offset, offset);
        }

        math::Vec3 jointPosition(const math::Mat4& joint)
        {
            return math::transformPoint(joint, math::Vec3{0.0f, 0.0f, 0.0f});
        }
    } //namespace

    Limb limbParent(Limb limb) noexcept
    {
        return isLimb(limb) ? k_parents[static_cast<std::size_t>(limb)] : Limb::None;
    }

    LimbMask limbBit(Limb limb) noexcept
    {
        return isLimb(limb) ? static_cast<LimbMask>(1u << static_cast<unsigned>(limb)) : LimbMask{0};
    }

    LimbMask limbSubtree(Limb limb) noexcept
    {
        LimbMask mask = 0;

        for (std::size_t index = 0; index < k_limbCount; ++index)
        {
            Limb walk = static_cast<Limb>(index);

            while (walk != Limb::None && walk != limb)
            {
                walk = limbParent(walk);
            }

            if (walk == limb && limb != Limb::None)
            {
                mask = static_cast<LimbMask>(mask | limbBit(static_cast<Limb>(index)));
            }
        }

        return mask;
    }

    const char* toString(Limb limb) noexcept
    {
        return isLimb(limb) ? k_names[static_cast<std::size_t>(limb)] : "none";
    }

    bool hasLimbs(const SkinnedModel& model) noexcept
    {
        return model.limbs.size() + 1 >= k_limbCount;
    }

    const LimbInfo* findLimb(const SkinnedModel& model, Limb limb) noexcept
    {
        for (const LimbInfo& info : model.limbs)
        {
            if (info.limb == limb)
            {
                return &info;
            }
        }

        return nullptr;
    }

    uint64_t limbParts(const SkinnedModel& model, LimbMask limbs, LimbPartRole role) noexcept
    {
        uint64_t mask = 0;

        for (std::size_t index = 0; index < model.parts.size(); ++index)
        {
            const SkinnedModelPart& part = model.parts[index];

            if (part.role == role && role != LimbPartRole::Whole && (limbs & limbBit(part.limb)) != 0)
            {
                mask |= partBit(index);
            }
        }

        return mask;
    }

    uint64_t woundCapParts(const SkinnedModel& model) noexcept
    {
        constexpr LimbMask k_everyLimb{static_cast<LimbMask>((1u << k_limbCount) - 1u)};

        return limbParts(model, k_everyLimb, LimbPartRole::StumpCap) | limbParts(model, k_everyLimb, LimbPartRole::PieceCap);
    }

    Limb jointLimb(const SkinnedModel& model, uint32_t joint) noexcept
    {
        if (!hasLimbs(model) || joint >= model.skeleton.joints.size())
        {
            return Limb::None;
        }

        for (uint32_t walk = joint; walk != k_noJoint; walk = model.skeleton.joints[walk].parent)
        {
            for (const LimbInfo& info : model.limbs)
            {
                if (info.rootJoint == walk)
                {
                    return info.limb;
                }
            }
        }

        return Limb::Torso;
    }

    Limb nearestLimb(const SkinnedModel& model, std::span<const math::Mat4> joints, const math::Vec3& point) noexcept
    {
        if (!hasLimbs(model) || joints.size() != model.skeleton.joints.size())
        {
            return Limb::None;
        }

        Limb nearest = Limb::Torso;
        float nearestDistance = std::numeric_limits<float>::max();

        for (std::size_t index = 0; index < joints.size(); ++index)
        {
            const uint32_t parent = model.skeleton.joints[index].parent;

            if (parent == k_noJoint)
            {
                continue;
            }

            const float distance = segmentDistanceSquared(point, jointPosition(joints[parent]),
                jointPosition(joints[index]));

            if (distance < nearestDistance)
            {
                nearestDistance = distance;
                nearest = jointLimb(model, parent);
            }
        }

        return nearest;
    }

    std::vector<math::Vec3> poseLimbHull(const SkinnedModel& model, LimbMask limbs, std::span<const math::Mat4> skin)
    {
        std::vector<math::Vec3> points;

        if (skin.size() != model.skeleton.joints.size())
        {
            return points;
        }

        for (const LimbInfo& info : model.limbs)
        {
            if ((limbs & limbBit(info.limb)) == 0)
            {
                continue;
            }

            for (const LimbHullPoint& hullPoint : info.hull)
            {
                math::Vec3 posed{0.0f, 0.0f, 0.0f};

                for (std::size_t influence = 0; influence < hullPoint.joints.size(); ++influence)
                {
                    const uint32_t joint = hullPoint.joints[influence];

                    if (hullPoint.weights[influence] > 0.0f && joint < skin.size())
                    {
                        posed += math::transformPoint(skin[joint], hullPoint.position) * hullPoint.weights[influence];
                    }
                }

                points.push_back(posed);
            }
        }

        return points;
    }
} //namespace lockstep::anim
