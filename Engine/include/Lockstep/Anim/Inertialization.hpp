#ifndef LOCKSTEP_ANIM_INERTIALIZATION_HPP
#define LOCKSTEP_ANIM_INERTIALIZATION_HPP

#include "Lockstep/Anim/Skeleton.hpp"
#include "Lockstep/Core/Math.hpp"

#include <vector>

namespace lockstep::anim
{
    // Inertial blending between motions: instead of evaluating and mixing two motions during a
    // crossfade, a transition switches to the new motion at once and keeps, per joint, the
    // difference between the pose that was shown and the new pose (position and rotation, and
    // their velocities). That offset is added on top of the new motion and decays like a
    // critically damped spring, so the shown pose moves on with the speed it had and settles into
    // the new motion without a pop or a floaty mix. Works on local joint transforms; scale is not
    // offset. Plain data, one per animated character (the Animator owns one).
    class PoseInertializer
    {
    public:
        // Starts a transition into a new motion. Does nothing before a pose was shown (apply()).
        // target:         the new motion's pose this frame.
        // targetBefore:   the new motion's pose one step earlier (for its velocity).
        // stepSeconds:    the time between targetBefore and target; 0 assumes the new motion is
        //                 still.
        // halflife:       seconds the offsets take to halve; 0 or less snaps to the new motion.
        void begin(const Pose& target, const Pose& targetBefore, float stepSeconds, float halflife);

        // Lets the offsets decay, adds them to a pose and remembers the result as the pose shown.
        // Call once per evaluated frame, after begin() on a transition frame.
        // pose:         the current motion's pose, changed in place to the shown pose.
        // deltaSeconds: time since the previous call.
        void apply(Pose& pose, float deltaSeconds);

        // Drops the offsets and the shown pose (after a teleport or a snap).
        void reset();

        // Returns: true while an offset is still noticeable.
        [[nodiscard]] bool isActive() const noexcept;

    private:
        void decay(float deltaSeconds);
        void remember(const Pose& pose, float deltaSeconds);

        std::vector<math::Vec3> m_positionOffsets;
        std::vector<math::Vec3> m_positionVelocities;
        std::vector<math::Vec3> m_rotationOffsets;
        std::vector<math::Vec3> m_rotationVelocities;
        Pose m_shown{};
        std::vector<math::Vec3> m_shownPositionVelocities;
        std::vector<math::Vec3> m_shownRotationVelocities;
        float m_halflife{0.0f};
        float m_activeSeconds{0.0f};
        bool m_isActive{false};
        bool m_isFresh{false};
    };
} //namespace lockstep::anim

#endif //LOCKSTEP_ANIM_INERTIALIZATION_HPP
