#include "Characters/CharacterMotion.hpp"

#include <Lockstep/Physics/PhysicsWorld.hpp>

#include <cmath>

namespace game
{
    namespace
    {
        namespace anim = lockstep::anim;
        namespace physics = lockstep::physics;

        constexpr math::Vec3 k_up{0.0f, 1.0f, 0.0f};
        constexpr float k_reprobeDistance{0.015f};

        bool hasMoved(const math::Vec3& from, const math::Vec3& to)
        {
            return std::abs(from.x - to.x) > k_reprobeDistance || std::abs(from.y - to.y) > k_reprobeDistance
                || std::abs(from.z - to.z) > k_reprobeDistance;
        }

        void probeGround(CharacterMotion& motion, const physics::PhysicsWorld& world,
            const std::array<math::Vec3, anim::k_footProbeCount>& probes, const anim::LocomotionPoseSettings& settings)
        {
            const float reach = settings.probeHeight + settings.probeDepth;

            for (std::size_t probe = 0; probe < probes.size(); ++probe)
            {
                if (motion.hasProbes && !hasMoved(motion.probedAt[probe], probes[probe]))
                {
                    continue;
                }

                const std::optional<physics::RaycastHit> hit = world.raycast(probes[probe] + k_up * settings.probeHeight,
                    -k_up, reach, physics::RaycastIgnore{.allControllers = true, .dynamicBodies = true});
                motion.ground[probe] = hit ? anim::FootGround{.isHit = true, .point = hit->point, .normal = hit->normal}
                                           : anim::FootGround{};
                motion.probedAt[probe] = probes[probe];
            }

            motion.hasProbes = true;
        }
    } //namespace

    void poseCharacter(anim::SkinnedInstance& instance, const CharacterModel& character, CharacterMotion& motion,
        const LocomotionTuning& tuning, const CharacterPoseFrame& frame)
    {
        const anim::SkinnedModel& model = character.skinned();
        instance.animate(model, frame.deltaSeconds, frame.offsets);

        if (!character.locomotionRig)
        {
            instance.finishPose(model);
            return;
        }

        const anim::LocomotionPoseInput input = locomotionPoseInput(frame.signals, frame.yaw, frame.placement,
            frame.isGrounded, frame.deltaSeconds);
        const std::span<math::Mat4> joints = instance.editJointMatrices();
        const std::array<math::Vec3, anim::k_footProbeCount> probes = anim::prepareLocomotionPose(model.skeleton,
            *character.locomotionRig, joints, input, tuning.pose, motion.pose);

        if (frame.physics != nullptr && tuning.pose.footIk)
        {
            probeGround(motion, *frame.physics, probes, tuning.pose);
        }
        else
        {
            motion.ground = {};
            motion.hasProbes = false;
        }

        anim::finishLocomotionPose(model.skeleton, *character.locomotionRig, joints, input, tuning.pose, motion.ground,
            motion.pose);
        instance.finishPose(model);
    }
} //namespace game
