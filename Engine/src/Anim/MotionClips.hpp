#ifndef LOCKSTEP_ANIM_MOTION_CLIPS_HPP
#define LOCKSTEP_ANIM_MOTION_CLIPS_HPP

#include "Lockstep/Anim/MotionDatabase.hpp"

#include <array>
#include <cstdint>
#include <span>
#include <vector>

namespace lockstep::anim
{
    // Joints of a sampled frame, model space: hips, left ankle, right ankle.
    using MotionJoints = std::array<math::Vec3, 3>;

    // A clip sampled at k_motionSampleRate for building a motion database.
    // roots:  the smoothed root per frame.
    // joints: hips and ankles per frame, model space (root motion included).
    struct SampledMotionClip
    {
        std::vector<MotionRoot> roots;
        std::vector<MotionJoints> joints;
    };

    // Samples a clip and measures its root: the hips' ground position and the turn of the hips
    // (relative to the first frame for one-shot clips, to the clip's mean for loops, so a stance
    // that holds the hips turned, as rifle clips do, still faces +Z; clips are authored facing +Z),
    // smoothed over a few frames.
    // model:     the model.
    // rig:       its locomotion rig.
    // clip:      the clip.
    // isLooping: the clip cycles (smoothing wraps around).
    // Returns:   the sampled clip, at least two frames.
    [[nodiscard]] SampledMotionClip sampleMotionClip(const SkinnedModel& model, const LocomotionRig& rig,
        const AnimationClip& clip, bool isLooping);

    // Makes an in-place copy of a clip: its hips channels are resampled with the root's motion
    // taken out, so the clip stays at the origin facing +Z.
    // model: the model.
    // hips:  the hips joint.
    // clip:  the clip with root motion.
    // roots: its roots from sampleMotionClip().
    // Returns: the copy, named after the clip with an "@inplace" suffix.
    [[nodiscard]] AnimationClip makeInPlaceClip(const SkinnedModel& model, uint32_t hips, const AnimationClip& clip,
        std::span<const MotionRoot> roots);

    // Rotates a vector about +Y.
    // vector: the vector.
    // yaw:    radians, positive turning +Z towards +X.
    // Returns: the rotated vector.
    [[nodiscard]] math::Vec3 rotateYaw(const math::Vec3& vector, float yaw);

    // The root of a clip any number of frames ahead: loops continue cycle after cycle, one-shot
    // clips carry on with their last velocity.
    // roots:     the clip's roots.
    // frame:     the frame, may be past the end.
    // isLooping: the clip cycles.
    // Returns:   the root.
    [[nodiscard]] MotionRoot rootAhead(std::span<const MotionRoot> roots, uint32_t frame, bool isLooping);
} //namespace lockstep::anim

#endif //LOCKSTEP_ANIM_MOTION_CLIPS_HPP
