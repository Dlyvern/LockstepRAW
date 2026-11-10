#ifndef LOCKSTEP_PHYSICS_RAGDOLL_SKELETON_HPP
#define LOCKSTEP_PHYSICS_RAGDOLL_SKELETON_HPP

#include "Lockstep/Anim/Skeleton.hpp"
#include "Lockstep/Core/Result.hpp"
#include "Lockstep/Physics/Ragdoll.hpp"

#include <span>
#include <string>
#include <vector>

namespace lockstep::physics
{
    // One ragdoll body of a profile. Joint names match a skeleton joint exactly or after a
    // "prefix:" / "prefix_" ("Spine" matches "mixamorig:Spine"); "A|B" names alternatives, the
    // first one the skeleton has wins (some Mixamo rigs lack Spine2). Directions are in character
    // space: x the character's left, y up, z forward (measured from the skeleton's bind pose).
    // Sizes are for a character whose root-to-top distance is RagdollProfile::referenceTorso
    // and scale with the actual one.
    // bone:          the joint the body follows.
    // parent:        the profile bone it hangs from (earlier in the profile); empty for the root.
    // axisEnd:       joint whose origin the bone points at (twist axis, default capsule end).
    // capsuleFrom:   joint where the capsule starts; empty for `bone`.
    // capsuleTo:     joint where the capsule ends; empty for `axisEnd`.
    // startOffset:   metres the capsule start moves towards its end (negative extends it).
    // endExtension:  metres the capsule end reaches past capsuleTo (hands, head top).
    // radius:        capsule radius, metres.
    // bendDirection: which way the joint bends (e.g. forward for hips, back for knees).
    // limits:        joint limits relative to the bind pose.
    struct RagdollBoneProfile
    {
        std::string bone;
        std::string parent;
        std::string axisEnd;
        std::string capsuleFrom;
        std::string capsuleTo;
        float startOffset{0.0f};
        float endExtension{0.0f};
        float radius{0.08f};
        math::Vec3 bendDirection{0.0f, 0.0f, 1.0f};
        RagdollJointLimits limits{};
    };

    // How to build a ragdoll from a humanoid skeleton.
    // bones:          the bodies, root first, parents before children.
    // leftMarker:     a joint on the character's left (sets the left axis with rightMarker).
    // rightMarker:    its mirror on the right.
    // topMarker:      a joint above the root (sets the up axis and the size).
    // referenceTorso: root-to-topMarker distance the profile sizes are made for, metres.
    // density:        kg per cubic metre of every body.
    struct RagdollProfile
    {
        std::vector<RagdollBoneProfile> bones;
        std::string leftMarker{"LeftUpLeg"};
        std::string rightMarker{"RightUpLeg"};
        std::string topMarker{"Head"};
        float referenceTorso{0.6f};
        float density{1000.0f};
    };

    // Returns: a 14-body profile for Mixamo skeletons: pelvis, lower torso, chest (Spine2, or
    //          Spine1 on rigs without it), head, upper and lower arms (hands ride on the
    //          forearms), thighs, shins and feet. Joints between bodies (Spine1, Neck, Shoulder)
    //          keep their animated local pose.
    [[nodiscard]] RagdollProfile mixamoRagdollProfile();

    // A ragdoll description made from a skeleton, and which joint each body drives.
    // description: ready for PhysicsWorld::createRagdoll() (settings and velocity at defaults).
    // joints:      per body, the skeleton joint it follows (what ragdoll poses drive).
    struct SkeletonRagdoll
    {
        RagdollDescription description;
        std::vector<uint32_t> joints;
    };

    // Builds a ragdoll for a skeleton in its current pose. Shapes and joint frames come from the
    // bind pose (inverse bind matrices), so limits are relative to it; bodies start where the
    // current pose puts the bones.
    // skeleton:      the skeleton.
    // profile:       which bodies to make.
    // jointMatrices: model-space joint matrices of the current pose (one per joint).
    // placement:     world transform of the model (position and rotation; scale should be 1).
    // Returns:       the ragdoll, or InvalidArgument if a profile joint (other than an axisEnd,
    //                which falls back to the bone's own Y axis) is missing from the skeleton, the
    //                profile is malformed, or the matrix count does not match the skeleton.
    [[nodiscard]] Result<SkeletonRagdoll> describeSkeletonRagdoll(const anim::Skeleton& skeleton,
        const RagdollProfile& profile, std::span<const math::Mat4> jointMatrices, const math::Transform& placement);
} //namespace lockstep::physics

#endif //LOCKSTEP_PHYSICS_RAGDOLL_SKELETON_HPP
