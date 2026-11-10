#include "Lockstep/Anim/BoneMask.hpp"

#include <algorithm>

namespace lockstep::anim
{
    BoneMask makeBoneMask(const Skeleton& skeleton, std::span<const uint32_t> roots, bool isModelSpaceRoot)
    {
        BoneMask mask{};
        mask.weights.assign(skeleton.joints.size(), 0.0f);

        for (std::size_t index = 0; index < skeleton.joints.size(); ++index)
        {
            const bool isRoot = std::find(roots.begin(), roots.end(), static_cast<uint32_t>(index)) != roots.end();
            const uint32_t parent = skeleton.joints[index].parent;
            const bool isBelowRoot = parent < index && mask.weights[parent] > 0.0f;

            if (isRoot || isBelowRoot)
            {
                mask.weights[index] = 1.0f;
            }

            if (isRoot && isModelSpaceRoot)
            {
                mask.modelSpaceRoots.push_back(static_cast<uint32_t>(index));
            }
        }

        return mask;
    }
} //namespace lockstep::anim
