#ifndef LOCKSTEP_GAME_NPCS_NPC_RAGDOLLS_HPP
#define LOCKSTEP_GAME_NPCS_NPC_RAGDOLLS_HPP

#include <Lockstep/Anim/Limbs.hpp>
#include <Lockstep/Anim/SkinnedModel.hpp>
#include <Lockstep/Core/Transform.hpp>
#include <Lockstep/Physics/RagdollSkeleton.hpp>

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace lockstep::physics
{
    class PhysicsWorld;
} //namespace lockstep::physics

namespace game
{
    namespace math = lockstep::math;

    // Ragdoll tunables (dev panel). Presentation only: each machine uses its own.
    // isEnabled:     dead NPCs become ragdolls; otherwise they tip over procedurally.
    // maxSimulating: ragdolls simulated at once. Ragdolls keep simulating (PhysX puts them to
    //                sleep once they lie still, which costs next to nothing); past the cap the
    //                oldest sleeping one freezes into a still pose, else the oldest that has
    //                simulated minSeconds (if none has, the new body tips over procedurally).
    // minSeconds:    World seconds a ragdoll simulates before the cap may freeze it while awake.
    // shotImpulse:   push of the killing shot on the body part it hit, kg m/s.
    // hitImpulse:    push of a bullet hitting a body, kg m/s.
    // shotLift:      upward share of the push direction.
    // blendSeconds:  fade from the last animated pose into the ragdoll.
    // physics:       material and solver settings of new ragdolls.
    struct RagdollTuning
    {
        bool isEnabled{true};
        uint32_t maxSimulating{32};
        float minSeconds{1.0f};
        float shotImpulse{110.0f};
        float hitImpulse{70.0f};
        float shotLift{0.2f};
        float blendSeconds{0.1f};
        lockstep::physics::RagdollSettings physics{};
    };

    // One ragdoll body's capsule in the body's own frame (the bone frame its pose reports).
    // start, end: the capsule axis' ends.
    // radius:     its radius, metres.
    struct RagdollCapsule
    {
        math::Vec3 start{0.0f, 0.0f, 0.0f};
        math::Vec3 end{0.0f, 0.0f, 0.0f};
        float radius{0.0f};
    };

    // RagdollAttachment::anchor of a body that simulates.
    inline constexpr uint32_t k_notAttached{0xFFFFFFFFu};

    // A severed body that no longer simulates but rides on the body it hung from, so skin weighted
    // across the cut stays on the character instead of stretching to where the limb came off.
    // anchor:   index of the body it rides on (k_notAttached while it simulates).
    // relative: its frame in the anchor's frame.
    struct RagdollAttachment
    {
        uint32_t anchor{k_notAttached};
        math::Transform relative{};
    };

    // One NPC body's ragdoll on this machine (never replicated).
    // id:        the physics ragdoll; null while frozen.
    // joints:    per ragdoll body, the skeleton joint it drives.
    // parents:   per body, the body it hangs from (k_noRagdollBody for the root).
    // attachments: per body, whether it was severed and rides on another body.
    // capsules:  per body, its collision capsule (bullets are traced against them).
    // poses:     per body, its world frame this frame (kept when frozen).
    // driven:    the joints fed to SkinnedInstance::updateDriven().
    // placement: the model's world transform when it died (drawn with it from then on).
    // order:     when it last started simulating (the oldest freezes first).
    // seconds:   World seconds since it died.
    // awakeSeconds: World seconds simulated since it last started simulating.
    // isAsleep:  PhysX put every body to sleep (as of this frame).
    struct NpcRagdoll
    {
        lockstep::physics::RagdollId id{};
        std::vector<uint32_t> joints;
        std::vector<uint32_t> parents;
        std::vector<RagdollAttachment> attachments;
        std::vector<RagdollCapsule> capsules;
        std::vector<math::Transform> poses;
        std::vector<lockstep::anim::DrivenJoint> driven;
        math::Transform placement{};
        uint64_t order{0};
        float seconds{0.0f};
        float awakeSeconds{0.0f};
        bool isAsleep{false};
    };

    // Ragdoll state shared by every NPC look.
    // tuning:    the tunables.
    // profile:   the Mixamo ragdoll profile, made on first use.
    // chestBody: index of the body a shot without a known limb pushes.
    // nextOrder: order of the next ragdoll.
    // failures:  ragdolls that could not be made (logged once).
    struct NpcRagdolls
    {
        RagdollTuning tuning{};
        std::optional<lockstep::physics::RagdollProfile> profile;
        uint32_t chestBody{0};
        uint64_t nextOrder{1};
        uint32_t failures{0};
    };

    // How a ragdoll starts moving.
    // direction: horizontal direction of the shot.
    // limb:      the limb it hit (its body is pushed), or None for the chest.
    // impulse:   strength of the push, kg m/s.
    struct RagdollPush
    {
        math::Vec3 direction{0.0f, 0.0f, -1.0f};
        lockstep::anim::Limb limb{lockstep::anim::Limb::None};
        float impulse{0.0f};
    };

    // Makes a ragdoll for a body that just died, seeded from its current pose and pushed along
    // the killing shot at the limb it hit.
    // physics:   the physics world.
    // ragdolls:  shared ragdoll state.
    // model:     the drawn model.
    // instance:  its instance, updated at least once (its joint matrices seed the bodies).
    // placement: the model's world transform.
    // push:      the killing shot.
    // Returns:   the ragdoll, or nothing if it could not be made (logged once).
    [[nodiscard]] std::optional<NpcRagdoll> startNpcRagdoll(lockstep::physics::PhysicsWorld& physics,
        NpcRagdolls& ragdolls, const lockstep::anim::SkinnedModel& model, const lockstep::anim::SkinnedInstance& instance,
        const math::Transform& placement, const RagdollPush& push);

    // Starts simulating a frozen ragdoll again from the pose it froze in (e.g. a bullet hit it).
    // physics:  the physics world.
    // ragdolls: shared ragdoll state.
    // model:    the drawn model.
    // instance: its instance, driven by the frozen pose.
    // ragdoll:  the frozen ragdoll.
    // Returns:  true if it simulates now.
    bool wakeNpcRagdoll(lockstep::physics::PhysicsWorld& physics, NpcRagdolls& ragdolls,
        const lockstep::anim::SkinnedModel& model, const lockstep::anim::SkinnedInstance& instance, NpcRagdoll& ragdoll);

    // Reads a ragdoll's bone frames for this frame and whether it sleeps.
    // physics:  the physics world.
    // ragdoll:  the ragdoll.
    // alpha:    the frame's interpolation alpha between ticks.
    // seconds:  World-scaled presentation seconds since the last frame.
    // Returns:  the joints to drive the model with.
    [[nodiscard]] std::span<const lockstep::anim::DrivenJoint> updateNpcRagdoll(
        lockstep::physics::PhysicsWorld& physics, NpcRagdoll& ragdoll, float alpha, float seconds);

    // Stops simulating a ragdoll; it keeps its last pose.
    // physics: the physics world.
    // ragdoll: the ragdoll.
    void freezeNpcRagdoll(lockstep::physics::PhysicsWorld& physics, NpcRagdoll& ragdoll);

    // Takes a severed limb's bodies out of a ragdoll (once): they stop simulating and from then on
    // keep their frame relative to the nearest body above the cut, so the body's skin weighted
    // to the limb's joints stays in place.
    // physics: the physics world.
    // model:   the drawn model.
    // ragdoll: the ragdoll.
    // limb:    the severed limb.
    void severNpcRagdollLimb(lockstep::physics::PhysicsWorld& physics, const lockstep::anim::SkinnedModel& model,
        NpcRagdoll& ragdoll, lockstep::anim::Limb limb);

    // ragdoll: a ragdoll.
    // Returns: true while it is simulated.
    [[nodiscard]] bool isSimulating(const NpcRagdoll& ragdoll) noexcept;

    // Where a ray first meets a ragdoll's capsules, as posed this frame.
    // body:     index of the body hit.
    // point:    world point of the hit.
    // distance: distance from the ray origin.
    struct RagdollRayHit
    {
        uint32_t body{0};
        math::Vec3 point{0.0f, 0.0f, 0.0f};
        float distance{0.0f};
    };

    // Traces a ray against a ragdoll's capsules (frozen ones too) at their poses of this frame.
    // ragdoll:     the ragdoll.
    // origin:      ray start.
    // direction:   unit ray direction.
    // maxDistance: longest hit distance.
    // Returns:     the nearest hit, or nothing.
    [[nodiscard]] std::optional<RagdollRayHit> traceNpcRagdoll(const NpcRagdoll& ragdoll, const math::Vec3& origin,
        const math::Vec3& direction, float maxDistance);

    // ragdoll: a ragdoll.
    // body:    index of one of its bodies.
    // Returns: the world centre of the body's capsule this frame (the ragdoll's placement for a
    //          body out of range).
    [[nodiscard]] math::Vec3 ragdollBodyCentre(const NpcRagdoll& ragdoll, uint32_t body);

    // The body a limb's hit pushes: the first body at or below the limb's root joint (the chest
    // for the torso or None).
    // ragdolls: shared ragdoll state.
    // model:    the model.
    // ragdoll:  its ragdoll.
    // limb:     the limb.
    // Returns:  the body index.
    [[nodiscard]] uint32_t ragdollBodyOfLimb(const NpcRagdolls& ragdolls, const lockstep::anim::SkinnedModel& model,
        const NpcRagdoll& ragdoll, lockstep::anim::Limb limb);

    // Pushes one body of a ragdoll (waking it); a frozen ragdoll starts simulating again first.
    // physics:  the physics world.
    // ragdolls: shared ragdoll state.
    // model:    the drawn model.
    // instance: its instance.
    // ragdoll:  the ragdoll.
    // body:     the body pushed.
    // impulse:  change of momentum, kg m/s.
    // point:    world point it acts at; nothing for the body's centre.
    void pushNpcRagdoll(lockstep::physics::PhysicsWorld& physics, NpcRagdolls& ragdolls,
        const lockstep::anim::SkinnedModel& model, const lockstep::anim::SkinnedInstance& instance, NpcRagdoll& ragdoll,
        uint32_t body, const math::Vec3& impulse, const std::optional<math::Vec3>& point);

#if LOCKSTEP_DEV_TOOLS
    // Draws the ragdoll controls of the Game dev panel.
    // ragdolls:   shared ragdoll state.
    // simulating: ragdolls simulated now.
    // asleep:     of them, asleep.
    // frozen:     ragdolls frozen into still poses.
    // physics:    the physics world (for its step cost).
    void npcRagdollsDevSection(NpcRagdolls& ragdolls, uint32_t simulating, uint32_t asleep, uint32_t frozen,
        const lockstep::physics::PhysicsWorld& physics);
#endif
} //namespace game

#endif //LOCKSTEP_GAME_NPCS_NPC_RAGDOLLS_HPP
