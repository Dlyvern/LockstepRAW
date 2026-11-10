#ifndef LOCKSTEP_GAME_NPCS_NPC_PRESENTATION_HPP
#define LOCKSTEP_GAME_NPCS_NPC_PRESENTATION_HPP

#include "Archetypes.hpp"
#include "Characters/Locomotion.hpp"
#include "Npcs/HitReactions.hpp"
#include "Npcs/NpcMessages.hpp"
#include "Npcs/NpcRagdolls.hpp"
#include "Weapons/DroppedWeapons.hpp"

#include <Lockstep/Anim/SkinnedModel.hpp>
#include <Lockstep/Core/Transform.hpp>
#include <Lockstep/World/Entities.hpp>

#include <array>
#include <cstdint>
#include <optional>
#include <vector>

namespace lockstep::render
{
    class Renderer;
} //namespace lockstep::render

namespace lockstep::physics
{
    class PhysicsWorld;
} //namespace lockstep::physics

namespace game
{
    struct PawnCharacters;
    struct Visuals;

    // One armed NPC as this machine draws it. Presentation only, never replicated.
    // entity:       the local entity it belongs to (the host's own, or the client's mirror).
    // instance:     pose and animator.
    // locomotion:   movement observed from where it is drawn.
    // isDrawn:      drawn this frame.
    // deathSeconds: presentation seconds since it was first drawn dead (drives the procedural
    //               fall when there is no ragdoll).
    // shotCount:    the NPC's shot counter as of its last draw (the weapon effects follow it).
    // ragdoll:      its body's ragdoll once dead (simulated; frozen only past the cap).
    // isRagdollTried: making the ragdoll was attempted.
    // isHeadBurst:  its head was blown apart (see Gore/HeadBursts.hpp): no head gear is drawn.
    // isWeaponDropped: its weapon left its hand (it died, or lost the arm holding it); the
    //               weapon parts are no longer drawn on it.
    // model:        the model it was last drawn with (null before, or as a box).
    // modelTransform: the model's world placement when it was last drawn without a ragdoll.
    // gearParts:    the parts its head gear look draws.
    // severedLimbs: limbs cut off it (see Gore/SeveredLimbs.hpp); their parts are hidden and
    //               stump caps shown. A ragdoll drops the bodies of these limbs.
    // reaction:     procedural hit reaction on top of its animation while alive.
    struct NpcLook
    {
        lockstep::world::EntityId entity{};
        lockstep::anim::SkinnedInstance instance{};
        LocomotionState locomotion{};
        bool isDrawn{false};
        float deathSeconds{0.0f};
        uint8_t shotCount{0};
        std::optional<NpcRagdoll> ragdoll{};
        bool isRagdollTried{false};
        bool isWeaponDropped{false};
        bool isHeadBurst{false};
        const lockstep::anim::SkinnedModel* model{nullptr};
        math::Transform modelTransform{};
        lockstep::anim::PartMask gearParts{lockstep::anim::k_allParts};
        lockstep::anim::LimbMask severedLimbs{0};
        HitReaction reaction{};
    };

    // The weapon found on one model NPCs are drawn with.
    // model:  the model (null for an unused entry).
    // weapon: its weapon, if it has one.
    struct NpcModelWeapon
    {
        const lockstep::anim::SkinnedModel* model{nullptr};
        std::optional<HeldWeapon> weapon{};
    };

    // Every armed NPC's look. Each NPC is drawn with the armed model of its body (view.body: the
    // squad the SWAT operator or the tactical outfit, mercs the robber; all share the SWAT
    // skeleton and clips); head gear varies them further.
    // looks:        one per NPC drawn last frame.
    // ragdolls:     ragdoll tunables and shared state.
    // weapons:      weapons dropped by dead NPCs.
    // heldWeapons:  the weapon found on each armed model drawn so far.
    // reactionRigs: the joints hit reactions turn, per armed model drawn so far.
    // reactionTuning: how hit reactions look.
    struct NpcLooks
    {
        std::vector<NpcLook> looks;
        NpcRagdolls ragdolls{};
        DroppedWeapons weapons{};
        std::array<NpcModelWeapon, k_npcBodyCount> heldWeapons{};
        std::array<ReactionRig, k_npcBodyCount> reactionRigs{};
        HitReactionTuning reactionTuning{};
    };

    // Starts a presentation frame of NPC drawing.
    // looks: the looks.
    void beginNpcLooks(NpcLooks& looks);

    // Queues one armed NPC for drawing: the armed model of view.body (the SWAT one when that body
    // isn't loaded; head gear by view.gearLook) playing the
    // relaxed rifle locomotion, or the aiming one while alert, squashed while crouched behind
    // cover; a dead one drops its weapon and becomes a ragdoll pushed along the killing shot at
    // the limb it hit (this machine only, never replicated) that keeps simulating (asleep once
    // still) unless too many simulate; without a ragdoll it tips over backwards. Its shot counter
    // is kept for the weapon effects (WeaponFx). Without characters it is a box.
    // renderer:  the renderer.
    // physics:   the physics world (ragdolls).
    // looks:     the looks.
    // characters: the pawn characters (armed body and clips), or null.
    // visuals:   the fallback boxes.
    // archetype: Archetype::Merc or Archetype::Squad.
    // entity:    the local entity (key of its look).
    // transform: its interpolated transform (pawn-like: centre, yaw, capsule height as y scale).
    // view:      dead, alert, shots, head gear, killing shot heading.
    // alpha:     the frame's interpolation alpha between ticks.
    // seconds:   presentation time since the last frame (World-scaled).
    void drawNpc(lockstep::render::Renderer& renderer, lockstep::physics::PhysicsWorld& physics, NpcLooks& looks,
        const PawnCharacters* characters, const Visuals& visuals, Archetype archetype,
        lockstep::world::EntityId entity, const lockstep::math::Transform& transform, const NpcView& view, float alpha,
        float seconds);

    // Ends a presentation frame: forgets the looks (and destroys the ragdolls) of NPCs not drawn
    // this frame, and moves and draws the dropped weapons.
    // looks:    the looks.
    // physics:  the physics world.
    // renderer: the renderer.
    // alpha:    the frame's interpolation alpha between ticks.
    // seconds:  World-scaled presentation seconds since the last frame.
    void endNpcLooks(NpcLooks& looks, lockstep::physics::PhysicsWorld& physics, lockstep::render::Renderer& renderer,
        float alpha, float seconds);

    // looks:   the looks.
    // model:   a model NPCs are drawn with.
    // Returns: the weapon it holds, or null if it holds none.
    [[nodiscard]] const HeldWeapon* npcHeldWeapon(NpcLooks& looks, const lockstep::anim::SkinnedModel& model);

    // The parts to draw of an NPC: its head gear look without severed limbs (stump caps shown),
    // and without its weapon once dropped.
    // looks:   the looks.
    // look:    the NPC's look.
    // Returns: the parts.
    [[nodiscard]] lockstep::anim::PartMask npcBodyParts(NpcLooks& looks, const NpcLook& look);

    // look:    an NPC look.
    // Returns: the world placement its model was last drawn at (its ragdoll's once it has one).
    [[nodiscard]] const math::Transform& drawnPlacement(const NpcLook& look);

    // Drops an NPC's weapon (once): it leaves the hand as a debris body.
    // physics: the physics world.
    // looks:   the looks.
    // look:    the NPC's look (drawn at least once).
    // linear:  the weapon's starting velocity, m/s.
    // angular: its starting spin, rad/s.
    void dropNpcWeapon(lockstep::physics::PhysicsWorld& physics, NpcLooks& looks, NpcLook& look,
        const math::Vec3& linear, const math::Vec3& angular);

    // Pushes one body of a dead NPC's ragdoll; a frozen ragdoll simulates again (freezing an
    // older one past the cap).
    // physics: the physics world.
    // looks:   the looks.
    // look:    the dead NPC's look (with a ragdoll).
    // body:    the ragdoll body pushed.
    // impulse: change of momentum, kg m/s.
    // point:   world point it acts at; nothing for the body's centre.
    void pushNpcBody(lockstep::physics::PhysicsWorld& physics, NpcLooks& looks, NpcLook& look, uint32_t body,
        const math::Vec3& impulse, const std::optional<math::Vec3>& point);

    // Makes a living NPC flinch from a hit (presentation only; see pushHitReaction()). Does nothing
    // for an NPC not drawn yet, dead or ragdolled.
    // looks:   the looks.
    // look:    the NPC's look.
    // hit:     the hit.
    void reactToNpcHit(NpcLooks& looks, NpcLook& look, const ReactionHit& hit);

#if LOCKSTEP_DEV_TOOLS
    // Draws the NPC ragdoll controls of the Game dev panel.
    // looks:   the looks.
    // physics: the physics world.
    void npcLooksDevSection(NpcLooks& looks, const lockstep::physics::PhysicsWorld& physics);
#endif
} //namespace game

#endif //LOCKSTEP_GAME_NPCS_NPC_PRESENTATION_HPP
