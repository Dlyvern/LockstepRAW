#ifndef LOCKSTEP_ANIM_SKELETON_HPP
#define LOCKSTEP_ANIM_SKELETON_HPP

#include "Lockstep/Core/Bounds.hpp"
#include "Lockstep/Core/Math.hpp"
#include "Lockstep/Core/Transform.hpp"

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace lockstep::anim
{
    // Joint::parent of a root joint.
    inline constexpr uint32_t k_noJoint{0xFFFFFFFFu};

    // Most joints a skeleton may have (the GPU skinning path indexes joints with this range).
    inline constexpr uint32_t k_maxJoints{256};

    // One bone of a skeleton.
    // name:        the joint's node name as authored (may be empty).
    // parent:      index of the parent joint, always lower than this joint's index, or k_noJoint.
    // rest:        local transform relative to the parent when no clip animates it.
    // inverseBind: maps model space at bind time into the joint's local space (glTF
    //              inverseBindMatrices).
    struct Joint
    {
        std::string name;
        uint32_t parent{k_noJoint};
        math::Transform rest{};
        math::Mat4 inverseBind{1.0f};
    };

    // The joint hierarchy of a skinned model, parents before children.
    // joints:     the joints.
    // rootMatrix: model-space placement of whatever the root joints hang from (the armature
    //             node in Blender exports); applied above every root joint.
    struct Skeleton
    {
        std::vector<Joint> joints;
        math::Mat4 rootMatrix{1.0f};
    };

    // Local transforms of every joint of one skeleton, in skeleton order. What clips are sampled
    // into and blended in.
    struct Pose
    {
        std::vector<math::Transform> joints;
    };

    // Makes the pose where every joint is at its rest transform.
    // skeleton: the skeleton.
    // pose:     receives the rest pose; resized to the joint count.
    void resetToRest(const Skeleton& skeleton, Pose& pose);

    // Computes each joint's model-space matrix from local transforms (rootMatrix included).
    // skeleton: the skeleton.
    // pose:     local transforms, one per joint.
    // model:    receives one matrix per joint; must have the joint count.
    void computeJointMatrices(const Skeleton& skeleton, const Pose& pose, std::span<math::Mat4> model);

    // Computes the skinning matrices the GPU applies to bind-pose vertices: model * inverseBind.
    // skeleton: the skeleton.
    // model:    model-space joint matrices from computeJointMatrices().
    // skin:     receives one matrix per joint; must have the joint count.
    void computeSkinMatrices(const Skeleton& skeleton, std::span<const math::Mat4> model, std::span<math::Mat4> skin);

    // Bounds a posed mesh fits in, from spheres around its joints.
    // model:   model-space joint matrices from computeJointMatrices(); the sphere centres.
    // skin:    skinning matrices from computeSkinMatrices(); their largest scale scales the radii.
    // radii:   per joint, how far the bind-pose vertices it moves reach from it, in mesh space (0
    //          for joints that move no vertex).
    // Returns: model-space bounds, empty if every radius is 0.
    [[nodiscard]] math::Aabb poseBounds(std::span<const math::Mat4> model, std::span<const math::Mat4> skin,
        std::span<const float> radii);

    // Finds a joint by name.
    // skeleton: the skeleton.
    // name:     the exact joint name.
    // Returns:  the joint's index, or k_noJoint.
    [[nodiscard]] uint32_t findJoint(const Skeleton& skeleton, std::string_view name);
} //namespace lockstep::anim

#endif //LOCKSTEP_ANIM_SKELETON_HPP
