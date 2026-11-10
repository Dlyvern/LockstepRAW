#include "RagdollState.hpp"

#include "PhysXConversions.hpp"

namespace lockstep::physics
{
    namespace
    {
        constexpr physx::PxU32 k_ragdollMarker{0x80000000u};
        constexpr physx::PxU32 k_ragdollIndexMask{0x7FFFFFFFu};
        constexpr physx::PxU32 k_selfCollisionFlag{1u};

        bool isRagdoll(const physx::PxFilterData& data) noexcept
        {
            return (data.word0 & k_ragdollMarker) != 0;
        }

        bool touchesOwnBody(const physx::PxFilterData& first, const physx::PxFilterData& second) noexcept
        {
            const bool selfCollision = (first.word3 & second.word3 & k_selfCollisionFlag) != 0;
            const bool ignored = (first.word1 & second.word2) != 0 || (second.word1 & first.word2) != 0;
            return selfCollision && !ignored;
        }

        bool isPlainStatic(physx::PxFilterObjectAttributes attributes) noexcept
        {
            return physx::PxGetFilterObjectType(attributes) == physx::PxFilterObjectType::eRIGID_STATIC
                && !physx::PxFilterObjectIsTrigger(attributes);
        }
    } //namespace

    void releaseRagdoll(RagdollSlot& slot) noexcept
    {
        for (RagdollPart& part : slot.parts)
        {
            if (part.joint != nullptr)
            {
                part.joint->release();
                part.joint = nullptr;
            }
        }

        for (RagdollPart& part : slot.parts)
        {
            if (part.actor != nullptr)
            {
                part.actor->release();
                part.actor = nullptr;
            }
        }

        if (slot.aggregate != nullptr)
        {
            slot.aggregate->release();
            slot.aggregate = nullptr;
        }

        slot.parts.clear();
    }

    void beginRagdollStep(RagdollSlot& slot) noexcept
    {
        for (RagdollPart& part : slot.parts)
        {
            part.previous = part.current;
        }
    }

    void endRagdollStep(RagdollSlot& slot) noexcept
    {
        for (RagdollPart& part : slot.parts)
        {
            if (part.actor != nullptr)
            {
                part.current = fromPx(part.actor->getGlobalPose());
            }
        }
    }

    physx::PxFilterData ragdollFilterData(uint32_t ragdollIndex, uint32_t body, uint32_t ignoredBodies,
        bool selfCollision) noexcept
    {
        return physx::PxFilterData(k_ragdollMarker | (ragdollIndex & k_ragdollIndexMask), 1u << body, ignoredBodies,
            selfCollision ? k_selfCollisionFlag : 0u);
    }

    physx::PxFilterFlags ragdollAwareFilterShader(physx::PxFilterObjectAttributes attributes0,
        physx::PxFilterData filterData0, physx::PxFilterObjectAttributes attributes1, physx::PxFilterData filterData1,
        physx::PxPairFlags& pairFlags, const void* constantBlock, physx::PxU32 constantBlockSize)
    {
        const bool isRagdoll0 = isRagdoll(filterData0);
        const bool isRagdoll1 = isRagdoll(filterData1);

        if (!isRagdoll0 && !isRagdoll1)
        {
            return physx::PxDefaultSimulationFilterShader(attributes0, filterData0, attributes1, filterData1, pairFlags,
                constantBlock, constantBlockSize);
        }

        if (isRagdoll0 && isRagdoll1)
        {
            if (filterData0.word0 == filterData1.word0 && !touchesOwnBody(filterData0, filterData1))
            {
                return physx::PxFilterFlag::eKILL;
            }
        }
        else if (!isPlainStatic(isRagdoll0 ? attributes1 : attributes0))
        {
            return physx::PxFilterFlag::eKILL;
        }

        pairFlags = physx::PxPairFlag::eCONTACT_DEFAULT;
        return physx::PxFilterFlag::eDEFAULT;
    }
} //namespace lockstep::physics
