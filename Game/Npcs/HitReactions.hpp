#ifndef LOCKSTEP_GAME_NPCS_HIT_REACTIONS_HPP
#define LOCKSTEP_GAME_NPCS_HIT_REACTIONS_HPP

#include <Lockstep/Anim/DrivenPose.hpp>
#include <Lockstep/Anim/SkinnedModel.hpp>
#include <Lockstep/Core/Math.hpp>
#include <Lockstep/Core/Transform.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace game
{
    namespace math = lockstep::math;

    // Body regions a hit reaction pushes, each a spring on a short bone chain.
    // Torso: spine chain (jolt). Head: neck and head (snap). ArmLeft/ArmRight: shoulder and upper
    // arm (twist). LegLeft/LegRight: thigh and shin (buckle).
    enum class ReactionRegion : uint8_t
    {
        Torso = 0,
        Head,
        ArmLeft,
        ArmRight,
        LegLeft,
        LegRight
    };

    // Number of reaction regions.
    inline constexpr std::size_t k_reactionRegionCount{6};

    // How hit reactions look, editable from the dev panel. Presentation only.
    // isEnabled:       reactions are shown at all.
    // frequency:       spring frequency, Hz (higher recovers faster).
    // damping:         damping ratio, 0..1 (1 settles without overshoot).
    // impulse:         angular speed a hit of referenceDamage gives its region, rad/s.
    // torsoShare:      share of a limb or head hit the torso also takes.
    // headshotScale:   factor on the impulse of a head hit.
    // maxAngle:        largest offset of one region, radians (sustained fire stacks up to it).
    // referenceDamage: damage of a full-strength hit (the player's shot).
    // staggerDamage:   a hit of at least this damage (or any leg hit) staggers.
    // staggerImpulse:  speed a stagger gives the fold spring, 1/s.
    // staggerFold:     spine fold and knee bend of a full stagger, radians.
    struct HitReactionTuning
    {
        bool isEnabled{true};
        float frequency{2.4f};
        float damping{0.55f};
        float impulse{7.0f};
        float torsoShare{0.45f};
        float headshotScale{1.6f};
        float maxAngle{0.7f};
        float referenceDamage{34.0f};
        float staggerDamage{30.0f};
        float staggerImpulse{9.0f};
        float staggerFold{0.35f};
    };

    // One region's spring: a model-space rotation vector (axis times angle) and its speed.
    struct ReactionSpring
    {
        math::Vec3 angle{0.0f, 0.0f, 0.0f};
        math::Vec3 velocity{0.0f, 0.0f, 0.0f};
    };

    // One joint a region turns and the share of the region's angle it takes.
    struct ReactionJoint
    {
        uint32_t joint{lockstep::anim::k_noJoint};
        float share{0.0f};
    };

    // A region's joints in a skeleton.
    // pivot:  where the region turns from (hips, neck, shoulder, hip joint).
    // tip:    the far end used to find the region nearest a hit.
    // joints: the chain it turns (up to three joints).
    struct ReactionRegionJoints
    {
        uint32_t pivot{lockstep::anim::k_noJoint};
        uint32_t tip{lockstep::anim::k_noJoint};
        std::array<ReactionJoint, 3> joints{};
    };

    // The joints hit reactions turn, found by Mixamo bone names once per model.
    // model:   the model they were found on (null before).
    // regions: per region; a region missing a joint stays still.
    // knees:   shin joints (left, right) a stagger bends.
    struct ReactionRig
    {
        const lockstep::anim::SkinnedModel* model{nullptr};
        std::array<ReactionRegionJoints, k_reactionRegionCount> regions{};
        std::array<uint32_t, 2> knees{lockstep::anim::k_noJoint, lockstep::anim::k_noJoint};
    };

    // One character's procedural hit reaction on top of its animation. Presentation only.
    // springs:         per region.
    // stagger, staggerSpeed: the fold spring (0 standing, 1 a full stumble).
    // staggerSide:     which knee buckles more: -1 left, 1 right, 0 both alike.
    // offsets:         scratch for the joint offsets handed to SkinnedInstance::update().
    struct HitReaction
    {
        std::array<ReactionSpring, k_reactionRegionCount> springs{};
        float stagger{0.0f};
        float staggerSpeed{0.0f};
        float staggerSide{0.0f};
        std::vector<lockstep::anim::JointRotationOffset> offsets;
    };

    // A hit to react to.
    // point:     world point it struck.
    // direction: unit world direction the shot travelled.
    // damage:    hit points it took (0 if unknown: counts as referenceDamage).
    struct ReactionHit
    {
        math::Vec3 point{0.0f, 0.0f, 0.0f};
        math::Vec3 direction{0.0f, 0.0f, -1.0f};
        int32_t damage{0};
    };

    // Finds the rig of a model (does nothing if `rig` already belongs to it).
    // rig:   the rig.
    // model: the model.
    void resolveReactionRig(ReactionRig& rig, const lockstep::anim::SkinnedModel& model);

    // Pushes the region nearest the hit (and the torso a little) in the shot direction; heavy and
    // leg hits also stagger.
    // reaction:  the character's reaction.
    // rig:       its model's rig.
    // joints:    model-space joint matrices of its current pose.
    // placement: the world placement it was drawn at.
    // hit:       the hit.
    // tuning:    the look.
    // Returns:   the region pushed.
    ReactionRegion pushHitReaction(HitReaction& reaction, const ReactionRig& rig, std::span<const math::Mat4> joints,
        const math::Transform& placement, const ReactionHit& hit, const HitReactionTuning& tuning);

    // Lets the springs recover and fills reaction.offsets for this frame.
    // reaction: the reaction.
    // rig:      its model's rig.
    // tuning:   the look.
    // seconds:  World seconds since the last frame (bullet-time slows reactions).
    void updateHitReaction(HitReaction& reaction, const ReactionRig& rig, const HitReactionTuning& tuning,
        float seconds);

    // Stops every spring (death, respawn).
    // reaction: the reaction.
    void resetHitReaction(HitReaction& reaction);

#if LOCKSTEP_DEV_TOOLS
    // Draws the hit reaction controls of the Game dev panel.
    // tuning: the look.
    void hitReactionDevSection(HitReactionTuning& tuning);
#endif
} //namespace game

#endif //LOCKSTEP_GAME_NPCS_HIT_REACTIONS_HPP
