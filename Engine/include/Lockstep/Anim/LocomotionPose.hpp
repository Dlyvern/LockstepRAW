#ifndef LOCKSTEP_ANIM_LOCOMOTION_POSE_HPP
#define LOCKSTEP_ANIM_LOCOMOTION_POSE_HPP

#include "Lockstep/Anim/JointEdits.hpp"
#include "Lockstep/Anim/Skeleton.hpp"
#include "Lockstep/Core/Math.hpp"
#include "Lockstep/Core/Transform.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>

namespace lockstep::anim
{
    // Procedural corrections that make clip-driven (or motion-matched) locomotion fit the world:
    // orientation warping (legs along the movement, upper body on the aim), stride warping (leg
    // reach matched to the ground speed), foot locking, lean, and foot IK on the ground with the
    // pelvis lowered so the lower foot reaches. Applied to model-space joint matrices between
    // SkinnedInstance::animate() and finishPose(), in two steps so the caller can probe the ground
    // where the warped feet land (raycasts stay with the caller):
    //   1. prepareLocomotionPose(): warps the lower body and returns where to probe.
    //   2. finishLocomotionPose(): takes the probe hits and applies lean, pelvis and leg IK.
    // Presentation only. Model space is assumed Y-up with the ground of the clips at y = 0.

    // Legs a locomotion rig has: index 0 is the left leg, 1 the right.
    inline constexpr std::size_t k_legCount{2};

    // Most spine joints the upper body counter-rotation is spread over.
    inline constexpr std::size_t k_maxSpineJoints{4};

    // Ground probes per frame: per leg the ankle (index leg * 2) and the ball of the foot
    // (leg * 2 + 1).
    inline constexpr std::size_t k_footProbeCount{4};

    // The joint name endings a rig is found by (compared against the end of each joint name, so
    // "Hips" matches "mixamorig:Hips"). Spine names may be left empty.
    struct LocomotionRigNames
    {
        std::string_view hips;
        std::array<std::string_view, k_maxSpineJoints> spine{};
        std::array<std::string_view, k_legCount> upperLegs{};
        std::array<std::string_view, k_legCount> lowerLegs{};
        std::array<std::string_view, k_legCount> feet{};
        std::array<std::string_view, k_legCount> toes{};
    };

    // One leg of a rig.
    // chain:        thigh, knee and ankle.
    // toe:          the ball of the foot (may be k_noJoint; the ankle is probed twice then).
    // ankleHeight:  the ankle's height above the ground in the bind pose, metres.
    struct LegJoints
    {
        TwoBoneChain chain{};
        uint32_t toe{k_noJoint};
        float ankleHeight{0.08f};
    };

    // The joints locomotion corrections move on one skeleton.
    // hips:       the pelvis; everything turned or moved with the body hangs below it.
    // spine:      the spine joints, lowest first, that turn the upper body back to the aim.
    // spineCount: how many entries of `spine` are used.
    // legs:       left and right leg.
    // forward:    the direction the model faces in model space.
    struct LocomotionRig
    {
        uint32_t hips{k_noJoint};
        std::array<uint32_t, k_maxSpineJoints> spine{};
        uint32_t spineCount{0};
        std::array<LegJoints, k_legCount> legs{};
        math::Vec3 forward{0.0f, 0.0f, 1.0f};
    };

    // Finds a rig on a skeleton by joint name endings and measures the ankle heights in the bind
    // pose.
    // skeleton: the skeleton.
    // names:    the name endings.
    // forward:  the direction the model faces in model space.
    // Returns:  the rig, or nothing when the hips or any leg joint is missing.
    [[nodiscard]] std::optional<LocomotionRig> findLocomotionRig(const Skeleton& skeleton,
        const LocomotionRigNames& names, const math::Vec3& forward = math::Vec3{0.0f, 0.0f, 1.0f});

    // What a ground probe found below a foot, world space.
    // isHit:  the ray hit something.
    // point:  where.
    // normal: the surface normal there.
    struct FootGround
    {
        bool isHit{false};
        math::Vec3 point{0.0f, 0.0f, 0.0f};
        math::Vec3 normal{0.0f, 1.0f, 0.0f};
    };

    // How the corrections behave (shared by many characters; dev-tunable).
    // footIk:          feet are placed on the probed ground; off keeps the clip's flat floor.
    // probeHeight:     probes start this far above the placement, metres (highest step reached).
    // probeDepth:      and reach this far below it, metres.
    // maxFootRaise:    the most a foot is raised above the placement's ground, metres.
    // maxFootDrop:     the most a foot is lowered below it, metres.
    // maxPelvisDrop:   the most the pelvis is lowered so the lower foot reaches, metres.
    // minGroundUp:     ground steeper than this (normal's up component) is ignored (walls).
    // maxFootTilt:     the most a foot tilts to the ground's slope, radians.
    // plantedHeight:   how far above its bind height an ankle may be for the foot to still take
    //                  the slope fully (lifted feet keep the clip's angle), metres.
    // footHalflife:    how quickly a foot follows a ground height change, seconds.
    // pelvisHalflife:  how quickly the pelvis follows, seconds.
    // fadeSeconds:     how long foot IK takes to come in or go (landing, ragdoll), seconds.
    // maxLowerBodyYaw: the most the lower body is turned from the facing, radians.
    // minStrideScale, maxStrideScale: limits of the stride scale.
    // maxLean:         the most the body leans, radians.
    // maxLockDistance: a locked foot further than this from where the clip puts it is released,
    //                  metres.
    struct LocomotionPoseSettings
    {
        bool footIk{true};
        float probeHeight{0.55f};
        float probeDepth{0.6f};
        float maxFootRaise{0.45f};
        float maxFootDrop{0.5f};
        float maxPelvisDrop{0.45f};
        float minGroundUp{0.6f};
        float maxFootTilt{0.45f};
        float plantedHeight{0.08f};
        float footHalflife{0.045f};
        float pelvisHalflife{0.07f};
        float fadeSeconds{0.2f};
        float maxLowerBodyYaw{1.5f};
        float minStrideScale{0.5f};
        float maxStrideScale{1.6f};
        float maxLean{0.35f};
        float maxLockDistance{0.45f};
    };

    // One character's corrections this frame, from its observed movement.
    // placement:     the world transform the model is drawn with.
    // lowerBodyYaw:  how far the hips and legs turn from the facing about model up, radians
    //                (positive turns towards model +X from +Z); the spine turns the upper body
    //                back by as much (orientation warping, turning in place).
    // moveDirection: the ground movement direction in model space (need not be normalised); zero
    //                when standing (no stride warping then).
    // strideScale:   how much longer the steps are than the clip's along moveDirection (1 = as
    //                authored).
    // lean:          the tilt of the body, model space: its direction is where the upper body
    //                leans to, its length the angle in radians (zero for none).
    // footLift:      per leg, metres the foot is lifted above where the clip puts it (stepping
    //                procedurally).
    // footLock:      per leg, 0 the foot follows the clip, 1 it stays where it was when the lock
    //                last was 0 (in world space), in between blends.
    // isGrounded:    the character stands on the ground; foot IK fades out when false (vaulting,
    //                kicking) and while no probe finds ground within maxFootDrop (airborne).
    // deltaSeconds:  presentation time since the last frame.
    struct LocomotionPoseInput
    {
        math::Transform placement{};
        float lowerBodyYaw{0.0f};
        math::Vec3 moveDirection{0.0f, 0.0f, 0.0f};
        float strideScale{1.0f};
        math::Vec3 lean{0.0f, 0.0f, 0.0f};
        std::array<float, k_legCount> footLift{};
        std::array<float, k_legCount> footLock{};
        bool isGrounded{true};
        float deltaSeconds{0.0f};
    };

    // Per character state of the corrections (smoothed ground, locked feet). Plain data.
    // feet:          this frame's warped ankle targets in model space (prepare to finish).
    // clipHeights:   this frame's ankle heights from the clip, model space.
    // footOffsets, footOffsetSpeeds: smoothed ground heights under the feet relative to the
    //                placement, metres, and their rates.
    // groundNormals: smoothed ground normals under the feet, model space.
    // lockedFeet:    world positions the feet are locked to.
    // hasLock:       lockedFeet are valid.
    // pelvisOffset, pelvisSpeed: smoothed pelvis drop and its rate.
    // weight:        foot IK fade, 0 to 1.
    // isPrepared:    prepareLocomotionPose() ran this frame.
    struct LocomotionPoseState
    {
        std::array<math::Vec3, k_legCount> feet{};
        std::array<float, k_legCount> clipHeights{};
        std::array<float, k_legCount> footOffsets{};
        std::array<float, k_legCount> footOffsetSpeeds{};
        std::array<math::Vec3, k_legCount> groundNormals{math::Vec3{0.0f, 1.0f, 0.0f}, math::Vec3{0.0f, 1.0f, 0.0f}};
        std::array<math::Vec3, k_legCount> lockedFeet{};
        std::array<bool, k_legCount> hasLock{};
        float pelvisOffset{0.0f};
        float pelvisSpeed{0.0f};
        float weight{0.0f};
        bool isPrepared{false};
    };

    // Step 1: turns the lower body (lowerBodyYaw, with the spine turning the upper body back),
    // computes the warped, lifted and locked foot targets, and returns where to probe the ground.
    // skeleton: the skeleton.
    // rig:      its locomotion rig.
    // model:    model-space joint matrices from SkinnedInstance::animate(), changed in place.
    // input:    this frame's corrections.
    // settings: the limits.
    // state:    the character's state.
    // Returns:  world points to probe below, k_footProbeCount of them (see k_footProbeCount): cast
    //           from point + up * probeHeight straight down for probeHeight + probeDepth.
    [[nodiscard]] std::array<math::Vec3, k_footProbeCount> prepareLocomotionPose(const Skeleton& skeleton,
        const LocomotionRig& rig, std::span<math::Mat4> model, const LocomotionPoseInput& input,
        const LocomotionPoseSettings& settings, LocomotionPoseState& state);

    // Step 2: leans the body, lowers the pelvis, bends the legs so the feet reach their targets
    // on the probed ground and tilts the planted feet to its slope.
    // skeleton: the skeleton.
    // rig:      its locomotion rig.
    // model:    the joint matrices prepareLocomotionPose() changed, changed in place.
    // input:    the input given to prepareLocomotionPose().
    // settings: the limits.
    // ground:   what each probe found, in the order prepareLocomotionPose() returned them.
    // state:    the character's state.
    void finishLocomotionPose(const Skeleton& skeleton, const LocomotionRig& rig, std::span<math::Mat4> model,
        const LocomotionPoseInput& input, const LocomotionPoseSettings& settings,
        std::span<const FootGround, k_footProbeCount> ground, LocomotionPoseState& state);
} //namespace lockstep::anim

#endif //LOCKSTEP_ANIM_LOCOMOTION_POSE_HPP
