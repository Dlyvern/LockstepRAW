#include "Lockstep/Anim/LocomotionPose.hpp"

#include "Lockstep/Core/Rotation.hpp"
#include "Lockstep/Core/Spring.hpp"

#include <algorithm>
#include <cmath>
#include <vector>

namespace lockstep::anim
{
    namespace
    {
        constexpr math::Vec3 k_up{0.0f, 1.0f, 0.0f};
        constexpr float k_tinyAngle{1.0e-4f};
        constexpr float k_tinyLength{1.0e-3f};
        constexpr float k_minAnkleHeight{0.02f};
        constexpr float k_ln2{0.69314718f};

        bool endsWith(std::string_view text, std::string_view suffix)
        {
            return text.size() >= suffix.size() && text.substr(text.size() - suffix.size()) == suffix;
        }

        uint32_t findEnding(const Skeleton& skeleton, std::string_view suffix)
        {
            if (suffix.empty())
            {
                return k_noJoint;
            }

            for (uint32_t joint = 0; joint < skeleton.joints.size(); ++joint)
            {
                if (endsWith(skeleton.joints[joint].name, suffix))
                {
                    return joint;
                }
            }

            return k_noJoint;
        }

        math::Vec3 planar(const math::Vec3& vector)
        {
            return math::Vec3{vector.x, 0.0f, vector.z};
        }

        bool isKnown(std::span<const math::Mat4> model, uint32_t joint)
        {
            return joint < model.size();
        }

        math::Vec3 lockFoot(LocomotionPoseState& state, std::size_t leg, const math::Vec3& target, float lock,
            const math::Mat4& placement, const math::Mat4& inversePlacement, float maxDistance)
        {
            const math::Vec3 worldTarget = math::transformPoint(placement, target);

            if (lock <= 0.0f || !state.hasLock[leg])
            {
                state.lockedFeet[leg] = worldTarget;
                state.hasLock[leg] = true;
                return target;
            }

            const math::Vec3 locked = math::transformPoint(inversePlacement, state.lockedFeet[leg]);

            if (math::length(locked - target) > maxDistance)
            {
                state.lockedFeet[leg] = worldTarget;
                return target;
            }

            return math::lerp(target, locked, std::min(lock, 1.0f));
        }

        void turnLowerBody(const Skeleton& skeleton, const LocomotionRig& rig, std::span<math::Mat4> model, float yaw)
        {
            if (std::abs(yaw) < k_tinyAngle)
            {
                return;
            }

            rotateJoints(skeleton, model, rig.hips, math::angleAxis(yaw, k_up), jointPosition(model[rig.hips]));
            const uint32_t count = std::min<uint32_t>(rig.spineCount, k_maxSpineJoints);

            if (count == 0)
            {
                return;
            }

            const math::Quat share = math::angleAxis(-yaw / static_cast<float>(count), k_up);

            for (uint32_t index = 0; index < count; ++index)
            {
                const uint32_t joint = rig.spine[index];

                if (isKnown(model, joint))
                {
                    rotateJoints(skeleton, model, joint, share, jointPosition(model[joint]));
                }
            }
        }

        math::Vec3 warpStride(const math::Vec3& ankle, const math::Vec3& hips, const math::Vec3& direction, float scale)
        {
            const math::Vec3 flat = planar(direction);

            if (math::length(flat) < k_tinyLength || std::abs(scale - 1.0f) < k_tinyLength)
            {
                return ankle;
            }

            const math::Vec3 along = math::normalize(flat);
            return ankle + along * (math::dot(planar(ankle - hips), along) * (scale - 1.0f));
        }

        struct GroundUnderFoot
        {
            bool isFound{false};
            float height{0.0f};
            math::Vec3 normal{k_up};
        };

        GroundUnderFoot groundUnderFoot(std::span<const FootGround, k_footProbeCount> ground, std::size_t leg,
            const math::Mat4& inversePlacement, const LocomotionPoseSettings& settings)
        {
            GroundUnderFoot best{};

            for (std::size_t probe = leg * 2; probe < leg * 2 + 2; ++probe)
            {
                const FootGround& hit = ground[probe];

                if (!hit.isHit || hit.normal.y < settings.minGroundUp)
                {
                    continue;
                }

                const float height = math::transformPoint(inversePlacement, hit.point).y;

                if (height < -settings.maxFootDrop)
                {
                    continue;
                }

                if (!best.isFound || height > best.height)
                {
                    best = GroundUnderFoot{.isFound = true,
                        .height = height,
                        .normal = math::normalize(math::transformDirection(inversePlacement, hit.normal))};
                }
            }

            return best;
        }

        void leanBody(const Skeleton& skeleton, const LocomotionRig& rig, std::span<math::Mat4> model,
            const math::Vec3& lean, float maxLean)
        {
            const math::Vec3 flat = planar(lean);
            const float angle = std::min(math::length(flat), maxLean);

            if (angle < k_tinyAngle)
            {
                return;
            }

            const math::Vec3 hips = jointPosition(model[rig.hips]);
            const math::Vec3 axis = math::normalize(math::cross(k_up, flat));
            rotateJoints(skeleton, model, rig.hips, math::angleAxis(angle, axis), planar(hips));
        }

        void tiltFoot(const Skeleton& skeleton, std::span<math::Mat4> model, const LegJoints& leg,
            const math::Vec3& normal, float amount, float maxTilt)
        {
            if (amount <= 0.0f || !isKnown(model, leg.chain.end))
            {
                return;
            }

            const math::Quat tilt = math::clampAngle(math::rotationBetween(k_up, normal), maxTilt);
            const math::Quat applied = math::slerp(math::Quat{}, tilt, std::min(amount, 1.0f));
            rotateJoints(skeleton, model, leg.chain.end, applied, jointPosition(model[leg.chain.end]));
        }
    } //namespace

    std::optional<LocomotionRig> findLocomotionRig(const Skeleton& skeleton, const LocomotionRigNames& names,
        const math::Vec3& forward)
    {
        LocomotionRig rig{.hips = findEnding(skeleton, names.hips), .forward = forward};

        if (rig.hips == k_noJoint)
        {
            return std::nullopt;
        }

        for (const std::string_view name : names.spine)
        {
            const uint32_t joint = findEnding(skeleton, name);

            if (joint != k_noJoint && rig.spineCount < k_maxSpineJoints)
            {
                rig.spine[rig.spineCount++] = joint;
            }
        }

        Pose rest;
        resetToRest(skeleton, rest);
        std::vector<math::Mat4> matrices(skeleton.joints.size(), math::Mat4{1.0f});
        computeJointMatrices(skeleton, rest, matrices);

        for (std::size_t leg = 0; leg < k_legCount; ++leg)
        {
            LegJoints& joints = rig.legs[leg];
            joints.chain = TwoBoneChain{.upper = findEnding(skeleton, names.upperLegs[leg]),
                .middle = findEnding(skeleton, names.lowerLegs[leg]),
                .end = findEnding(skeleton, names.feet[leg])};
            joints.toe = findEnding(skeleton, names.toes[leg]);

            if (joints.chain.upper == k_noJoint || joints.chain.middle == k_noJoint || joints.chain.end == k_noJoint)
            {
                return std::nullopt;
            }

            joints.ankleHeight = std::max(jointPosition(matrices[joints.chain.end]).y, k_minAnkleHeight);
        }

        return rig;
    }

    std::array<math::Vec3, k_footProbeCount> prepareLocomotionPose(const Skeleton& skeleton, const LocomotionRig& rig,
        std::span<math::Mat4> model, const LocomotionPoseInput& input, const LocomotionPoseSettings& settings,
        LocomotionPoseState& state)
    {
        const math::Mat4 placement = math::toMatrix(input.placement);
        std::array<math::Vec3, k_footProbeCount> probes{};
        probes.fill(input.placement.position);
        state.isPrepared = false;

        if (!isKnown(model, rig.hips))
        {
            return probes;
        }

        const math::Mat4 inversePlacement = math::inverse(placement);
        const float yaw = std::clamp(input.lowerBodyYaw, -settings.maxLowerBodyYaw, settings.maxLowerBodyYaw);
        turnLowerBody(skeleton, rig, model, yaw);
        const math::Vec3 hips = jointPosition(model[rig.hips]);
        const float strideScale = std::clamp(input.strideScale, settings.minStrideScale, settings.maxStrideScale);

        for (std::size_t leg = 0; leg < k_legCount; ++leg)
        {
            const LegJoints& joints = rig.legs[leg];

            if (!isKnown(model, joints.chain.end))
            {
                continue;
            }

            const math::Vec3 ankle = jointPosition(model[joints.chain.end]);
            const math::Vec3 toe = isKnown(model, joints.toe) ? jointPosition(model[joints.toe]) : ankle;
            math::Vec3 target = warpStride(ankle, hips, input.moveDirection, strideScale);
            state.clipHeights[leg] = target.y;
            target.y += std::max(input.footLift[leg], 0.0f);
            target = lockFoot(state, leg, target, input.footLock[leg], placement, inversePlacement,
                settings.maxLockDistance);
            state.feet[leg] = target;
            probes[leg * 2] = math::transformPoint(placement, planar(target));
            probes[leg * 2 + 1] = math::transformPoint(placement, planar(toe + (target - ankle)));
        }

        state.isPrepared = true;
        return probes;
    }

    void finishLocomotionPose(const Skeleton& skeleton, const LocomotionRig& rig, std::span<math::Mat4> model,
        const LocomotionPoseInput& input, const LocomotionPoseSettings& settings,
        std::span<const FootGround, k_footProbeCount> ground, LocomotionPoseState& state)
    {
        if (!state.isPrepared || !isKnown(model, rig.hips))
        {
            return;
        }

        state.isPrepared = false;
        const float delta = std::max(input.deltaSeconds, 0.0f);
        const math::Mat4 inversePlacement = math::inverse(math::toMatrix(input.placement));
        const float normalBlend = settings.footHalflife > 0.0f ? 1.0f - std::exp(-delta * k_ln2 / settings.footHalflife)
                                                                : 1.0f;
        bool hasGround = false;

        for (std::size_t leg = 0; leg < k_legCount; ++leg)
        {
            const GroundUnderFoot found = groundUnderFoot(ground, leg, inversePlacement, settings);
            hasGround = hasGround || found.isFound;
            const float goal = found.isFound ? std::clamp(found.height, -settings.maxFootDrop, settings.maxFootRaise) : 0.0f;
            math::criticalSpring(state.footOffsets[leg], state.footOffsetSpeeds[leg], goal, settings.footHalflife, delta);
            state.groundNormals[leg] = math::normalize(math::lerp(state.groundNormals[leg], found.normal, normalBlend));
        }

        const float fadeTarget = settings.footIk && input.isGrounded && hasGround ? 1.0f : 0.0f;
        const float fadeStep = settings.fadeSeconds > 0.0f ? delta / settings.fadeSeconds : 1.0f;
        state.weight = fadeTarget > state.weight ? std::min(state.weight + fadeStep, fadeTarget)
                                                 : std::max(state.weight - fadeStep, fadeTarget);
        const float pelvisGoal = std::clamp(std::min(state.footOffsets[0], state.footOffsets[1]), -settings.maxPelvisDrop,
            settings.maxFootRaise);
        math::criticalSpring(state.pelvisOffset, state.pelvisSpeed, pelvisGoal, settings.pelvisHalflife, delta);
        const float weight = state.weight;
        leanBody(skeleton, rig, model, input.lean, settings.maxLean);

        if (weight > 0.0f)
        {
            translateJoints(skeleton, model, rig.hips, k_up * (state.pelvisOffset * weight));
        }

        const float yaw = std::clamp(input.lowerBodyYaw, -settings.maxLowerBodyYaw, settings.maxLowerBodyYaw);
        const math::Vec3 pole = math::angleAxis(yaw, k_up) * rig.forward;

        for (std::size_t leg = 0; leg < k_legCount; ++leg)
        {
            const LegJoints& joints = rig.legs[leg];

            if (!isKnown(model, joints.chain.end))
            {
                continue;
            }

            const math::Vec3 target = state.feet[leg] + k_up * (state.footOffsets[leg] * weight);

            if (math::length(jointPosition(model[joints.chain.end]) - target) > k_tinyLength * 0.1f)
            {
                static_cast<void>(solveTwoBoneIk(skeleton, model, joints.chain, target, pole));
            }

            const float lifted = settings.plantedHeight > 0.0f
                ? (state.clipHeights[leg] - joints.ankleHeight) / settings.plantedHeight : 0.0f;
            const float planted = 1.0f - std::clamp(lifted, 0.0f, 1.0f);
            tiltFoot(skeleton, model, joints, state.groundNormals[leg], weight * planted, settings.maxFootTilt);
        }
    }
} //namespace lockstep::anim
