#ifndef LOCKSTEP_ANIM_ROOT_MOTION_HPP
#define LOCKSTEP_ANIM_ROOT_MOTION_HPP

#include "Lockstep/Anim/AnimationClip.hpp"
#include "Lockstep/Anim/Skeleton.hpp"
#include "Lockstep/Core/Math.hpp"

#include <cstdint>

namespace lockstep::anim
{
    // The joint a clip's turning is measured on and taken out at (the hips of a "turn 90 degrees"
    // clip), so the turn can be applied by the caller instead (turning in place, motion matching).
    // joint: index into the skeleton, or k_noJoint for none.
    // up:    model-space up (+Y) expressed in the joint's parent space at rest.
    struct RootJoint
    {
        uint32_t joint{k_noJoint};
        math::Vec3 up{0.0f, 1.0f, 0.0f};
    };

    // skeleton: the skeleton.
    // joint:    the root joint (usually the hips); k_noJoint gives an unused RootJoint.
    // Returns:  the root joint with its up axis.
    [[nodiscard]] RootJoint makeRootJoint(const Skeleton& skeleton, uint32_t joint);

    // How far a clip has turned its root joint about model up since its first frame.
    // clip:    the clip.
    // root:    the root joint.
    // seconds: time in the clip.
    // Returns: radians, positive turning +Z towards +X; 0 when the clip doesn't rotate the root.
    [[nodiscard]] float clipRootYaw(const AnimationClip& clip, const RootJoint& root, float seconds);

    // Turns the root joint of a pose about model up (e.g. by minus clipRootYaw() to keep a turning
    // clip facing its start).
    // pose: the pose, changed in place.
    // root: the root joint; unused or unknown does nothing.
    // yaw:  radians, positive turning +Z towards +X.
    void turnRoot(Pose& pose, const RootJoint& root, float yaw);
} //namespace lockstep::anim

#endif //LOCKSTEP_ANIM_ROOT_MOTION_HPP
