#ifndef LOCKSTEP_ANIM_DRIVEN_POSE_HPP
#define LOCKSTEP_ANIM_DRIVEN_POSE_HPP

#include "Lockstep/Anim/Skeleton.hpp"
#include "Lockstep/Core/Transform.hpp"

#include <cstdint>
#include <span>

namespace lockstep::anim
{
    // A joint placed in world space from outside the animator, e.g. by a ragdoll body.
    // joint: index into the skeleton.
    // world: the joint's world position and rotation. Scale is ignored: the joint keeps the
    //        scale its local transforms give it.
    struct DrivenJoint
    {
        uint32_t joint{k_noJoint};
        math::Transform world{};
    };

    // A rotation added on top of an animated joint (procedural hit reactions, aim offsets, look-at),
    // in model space and about the joint's own position; the joints below it follow.
    // joint:    index into the skeleton.
    // rotation: the added rotation in model space (identity for none).
    struct JointRotationOffset
    {
        uint32_t joint{k_noJoint};
        math::Quat rotation{};
    };

    // Computes model-space joint matrices from local transforms and adds rotations on top: each
    // offset joint is turned about its own position after its parent (offsets of ancestors and
    // descendants stack).
    // skeleton: the skeleton.
    // pose:     local transforms, one per joint.
    // offsets:  the added rotations, any order; unknown joint indices are ignored.
    // model:    receives one matrix per joint; must have the joint count.
    void computeOffsetJointMatrices(const Skeleton& skeleton, const Pose& pose,
        std::span<const JointRotationOffset> offsets, std::span<math::Mat4> model);

    // Computes model-space joint matrices where some joints are placed in world space and every
    // other joint follows its parent with its local transform from `pose` (so fingers ride on a
    // ragdoll's forearm in the pose they had).
    // skeleton:  the skeleton.
    // pose:      local transforms of the joints that are not driven.
    // placement: world transform the model is drawn with (maps model space to world space).
    // driven:    the world-placed joints, any order; unknown joint indices are ignored.
    // model:     receives one matrix per joint; must have the joint count.
    void computeDrivenJointMatrices(const Skeleton& skeleton, const Pose& pose, const math::Transform& placement,
        std::span<const DrivenJoint> driven, std::span<math::Mat4> model);

    // Blends two model-space joint matrices (position and scale linearly, rotation along the
    // shortest arc), e.g. to fade from an animated pose into a ragdoll.
    // from:    the matrix at weight 0.
    // to:      the matrix at weight 1.
    // weight:  0 to 1.
    // Returns: the blended matrix.
    [[nodiscard]] math::Mat4 blendJointMatrix(const math::Mat4& from, const math::Mat4& to, float weight);
} //namespace lockstep::anim

#endif //LOCKSTEP_ANIM_DRIVEN_POSE_HPP
