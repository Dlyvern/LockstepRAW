#ifndef LOCKSTEP_ANIM_LIMBS_HPP
#define LOCKSTEP_ANIM_LIMBS_HPP

#include "Lockstep/Anim/Skeleton.hpp"
#include "Lockstep/Core/Math.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace lockstep::anim
{
    struct SkinnedModel;

    // The regions a humanoid skinned model is cut into for dismemberment (LockstepCooker --limbs
    // assigns every triangle to the region of its dominant joints). The torso is the root; every
    // other limb hangs from the one limbParent() names. The values are stored in cooked .model
    // files: append, never reorder.
    enum class Limb : uint8_t
    {
        Torso = 0,
        Head,
        UpperArmLeft,
        ForearmLeft,
        UpperArmRight,
        ForearmRight,
        ThighLeft,
        ShinLeft,
        ThighRight,
        ShinRight,
        None = 0xFF
    };

    // Number of limbs (Torso to ShinRight).
    inline constexpr std::size_t k_limbCount{10};

    // A set of limbs: bit i stands for Limb i.
    using LimbMask = uint16_t;

    // What a skinned model part is in the limb split.
    // Whole:    geometry that was not split (a model cooked without --limbs); always drawn.
    // Region:   geometry of one limb's region.
    // StumpCap: the wound closing the body side of a limb's cut, drawn on the body once the limb
    //           is severed. Hidden by default.
    // PieceCap: the wound closing the limb side of the cut, drawn on the severed piece. Hidden by
    //           default.
    // The values are stored in cooked .model files: append, never reorder.
    enum class LimbPartRole : uint8_t
    {
        Whole = 0,
        Region,
        StumpCap,
        PieceCap
    };

    // One point of a limb's collision hull: a bind-pose vertex with its skinning, so the hull of
    // a severed limb can be posed like the mesh.
    // position: bind-pose position in mesh space.
    // joints:   indices into the skeleton's joints.
    // weights:  influence of each joint; they sum to 1.
    struct LimbHullPoint
    {
        math::Vec3 position{0.0f, 0.0f, 0.0f};
        std::array<uint8_t, 4> joints{};
        std::array<float, 4> weights{};
    };

    // One limb of a split model.
    // limb:      which limb.
    // rootJoint: the joint where the limb starts (e.g. the upper arm joint), where it is cut.
    // hull:      extreme points of the limb's own region (not of the limbs hanging from it), for a
    //            convex collision hull.
    struct LimbInfo
    {
        Limb limb{Limb::Torso};
        uint32_t rootJoint{k_noJoint};
        std::vector<LimbHullPoint> hull;
    };

    // limb:    a limb.
    // Returns: the limb it hangs from (Torso for the head, upper arms and thighs), or None for the
    //          torso and None.
    [[nodiscard]] Limb limbParent(Limb limb) noexcept;

    // limb:    a limb (not None).
    // Returns: its bit in a LimbMask; 0 for None.
    [[nodiscard]] LimbMask limbBit(Limb limb) noexcept;

    // limb:    a limb.
    // Returns: the limb and every limb hanging from it, directly or not (an upper arm takes its
    //          forearm along).
    [[nodiscard]] LimbMask limbSubtree(Limb limb) noexcept;

    // Returns: a readable name for `limb`, for logs and the dev panel.
    [[nodiscard]] const char* toString(Limb limb) noexcept;

    // model:   a skinned model.
    // Returns: true if the model was cooked with limbs (it has LimbInfo for every limb but the
    //          torso).
    [[nodiscard]] bool hasLimbs(const SkinnedModel& model) noexcept;

    // model:   a skinned model.
    // limb:    a limb.
    // Returns: the limb's info, or null if the model has none for it.
    [[nodiscard]] const LimbInfo* findLimb(const SkinnedModel& model, Limb limb) noexcept;

    // Selects parts of a model by limb and role, for SkinnedInstance::submit().
    // model:   a skinned model.
    // limbs:   the limbs whose parts count.
    // role:    the role the parts must have (Whole parts belong to no limb and are never selected).
    // Returns: the parts' mask (parts past the 64th are left out).
    [[nodiscard]] uint64_t limbParts(const SkinnedModel& model, LimbMask limbs, LimbPartRole role) noexcept;

    // model:   a skinned model.
    // Returns: every wound cap part (StumpCap and PieceCap), which a default draw leaves out.
    [[nodiscard]] uint64_t woundCapParts(const SkinnedModel& model) noexcept;

    // model:   a skinned model with limbs.
    // joint:   index into the skeleton's joints.
    // Returns: the limb the joint moves (the limb of the nearest limb root at or above it, else
    //          the torso); None for a model without limbs or a joint out of range.
    [[nodiscard]] Limb jointLimb(const SkinnedModel& model, uint32_t joint) noexcept;

    // Finds the limb nearest to a point, by distance to the bones of a pose (the segment from
    // each joint to its parent belongs to the parent's limb).
    // model:   a skinned model with limbs.
    // joints:  model-space joint matrices of the pose (SkinnedInstance::jointMatrices()).
    // point:   the point in model space.
    // Returns: the nearest limb; None for a model without limbs or a pose of the wrong size.
    [[nodiscard]] Limb nearestLimb(const SkinnedModel& model, std::span<const math::Mat4> joints,
        const math::Vec3& point) noexcept;

    // Poses the hull points of limbs, like the GPU poses their mesh.
    // model:   a skinned model with limbs.
    // limbs:   the limbs whose hull points to pose.
    // skin:    skinning matrices of the pose (SkinnedInstance::skinMatrices()).
    // Returns: the posed points in model space; empty for a pose of the wrong size.
    [[nodiscard]] std::vector<math::Vec3> poseLimbHull(const SkinnedModel& model, LimbMask limbs,
        std::span<const math::Mat4> skin);
} //namespace lockstep::anim

#endif //LOCKSTEP_ANIM_LIMBS_HPP
