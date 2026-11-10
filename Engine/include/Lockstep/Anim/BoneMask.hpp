#ifndef LOCKSTEP_ANIM_BONE_MASK_HPP
#define LOCKSTEP_ANIM_BONE_MASK_HPP

#include "Lockstep/Anim/Skeleton.hpp"

#include <cstdint>
#include <span>
#include <vector>

namespace lockstep::anim
{
    // Which joints an Animator override layer takes over (see Animator::playLayer()), e.g. the
    // upper body of a standing rifle clip on top of crouched legs.
    // weights:         per joint, how much of the layer's pose it takes: 0 keeps the base pose,
    //                  1 takes the layer's. Missing entries count as 0.
    // modelSpaceRoots: joints whose layer rotation is kept in model space rather than relative to
    //                  their parent: they face the way they face in the layer clip whatever the
    //                  base pose does below them (a torso keeps its aim while the hips crouch).
    struct BoneMask
    {
        std::vector<float> weights;
        std::vector<uint32_t> modelSpaceRoots;
    };

    // Makes a mask of whole joint subtrees.
    // skeleton:         the skeleton.
    // roots:            the joints whose subtrees (themselves and every joint below) the layer
    //                   takes over; unknown indices are ignored.
    // isModelSpaceRoot: true keeps the roots' layer rotations in model space
    //                   (BoneMask::modelSpaceRoots).
    // Returns:          the mask, weight 1 inside the subtrees and 0 elsewhere.
    [[nodiscard]] BoneMask makeBoneMask(const Skeleton& skeleton, std::span<const uint32_t> roots,
        bool isModelSpaceRoot);
} //namespace lockstep::anim

#endif //LOCKSTEP_ANIM_BONE_MASK_HPP
