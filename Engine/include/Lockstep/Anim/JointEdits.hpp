#ifndef LOCKSTEP_ANIM_JOINT_EDITS_HPP
#define LOCKSTEP_ANIM_JOINT_EDITS_HPP

#include "Lockstep/Anim/Skeleton.hpp"
#include "Lockstep/Core/Math.hpp"

#include <cstdint>
#include <span>

namespace lockstep::anim
{
    // Edits of a posed skeleton in model space, applied to the joint matrices after the animated
    // pose is computed (see SkinnedInstance::animate()). Moving a joint moves every joint below
    // it, so IK, warping and lean can stack in any order.

    // Three joints of a limb solved together: upper (thigh, upper arm), middle (knee, elbow) and
    // end (ankle, wrist). middle is a child of upper and end a child of middle (directly or not).
    struct TwoBoneChain
    {
        uint32_t upper{k_noJoint};
        uint32_t middle{k_noJoint};
        uint32_t end{k_noJoint};
    };

    // matrix:  a model-space joint matrix.
    // Returns: the joint's model-space position.
    [[nodiscard]] math::Vec3 jointPosition(const math::Mat4& matrix) noexcept;

    // matrix:  a model-space joint matrix (scale is ignored).
    // Returns: the joint's model-space rotation.
    [[nodiscard]] math::Quat jointRotation(const math::Mat4& matrix) noexcept;

    // Turns a joint and every joint below it about a model-space point.
    // skeleton: the skeleton.
    // model:    model-space joint matrices, changed in place.
    // joint:    the joint; an unknown index does nothing.
    // rotation: the model-space rotation.
    // pivot:    the model-space point it turns about (e.g. the joint's own position).
    void rotateJoints(const Skeleton& skeleton, std::span<math::Mat4> model, uint32_t joint, const math::Quat& rotation,
        const math::Vec3& pivot);

    // Moves a joint and every joint below it.
    // skeleton: the skeleton.
    // model:    model-space joint matrices, changed in place.
    // joint:    the joint; an unknown index does nothing.
    // offset:   the model-space move.
    void translateJoints(const Skeleton& skeleton, std::span<math::Mat4> model, uint32_t joint, const math::Vec3& offset);

    // Bends a limb so its end joint reaches a target (or points at it as far as the bones reach),
    // keeping the plane it bends in; the end joint keeps its model-space rotation.
    // skeleton: the skeleton.
    // model:    model-space joint matrices, changed in place.
    // chain:    the limb's joints; unknown indices do nothing.
    // target:   where the end joint should go, model space.
    // pole:     the direction the middle joint bends towards, used when the limb is straight
    //           (e.g. a knee bends forward), model space.
    // Returns:  how far the end joint stays from the target, metres (0 when it reached it).
    float solveTwoBoneIk(const Skeleton& skeleton, std::span<math::Mat4> model, const TwoBoneChain& chain,
        const math::Vec3& target, const math::Vec3& pole);
} //namespace lockstep::anim

#endif //LOCKSTEP_ANIM_JOINT_EDITS_HPP
