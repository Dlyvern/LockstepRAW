#ifndef LOCKSTEP_GAME_NPCS_NPC_PRESENTATION_HPP
#define LOCKSTEP_GAME_NPCS_NPC_PRESENTATION_HPP

#include "Archetypes.hpp"
#include "Characters/Locomotion.hpp"
#include "Npcs/NpcMessages.hpp"

#include <Lockstep/Anim/SkinnedModel.hpp>
#include <Lockstep/Core/Transform.hpp>
#include <Lockstep/World/Entities.hpp>

#include <cstdint>
#include <optional>
#include <vector>

namespace lockstep::render
{
    class Renderer;
} //namespace lockstep::render

namespace game
{
    struct PawnCharacters;
    struct Visuals;

    // One armed NPC as this machine draws it. Presentation only, never replicated.
    // entity:       the local entity it belongs to (the host's own, or the client's mirror).
    // instance:     pose and animator.
    // locomotion:   movement observed from where it is drawn.
    // isDrawn:      drawn this frame.
    // deathSeconds: presentation seconds since it was first drawn dead (drives the fall).
    // shotCount:    the NPC's shot counter last seen.
    // flashSeconds: what is left of the current muzzle flash.
    struct NpcLook
    {
        lockstep::world::EntityId entity{};
        lockstep::anim::SkinnedInstance instance{};
        LocomotionState locomotion{};
        bool isDrawn{false};
        float deathSeconds{0.0f};
        uint8_t shotCount{0};
        float flashSeconds{0.0f};
    };

    // Every armed NPC's look, and the merc body: the armed SWAT model with one flat charcoal
    // material on every part (a placeholder look that tells mercs from the squad at a glance).
    // looks:            one per NPC drawn last frame.
    // mercModel:        the merc body, made on first use.
    // isMercModelTried: making it was attempted (a failure is logged once; mercs then use the
    //                   squad's look).
    struct NpcLooks
    {
        std::vector<NpcLook> looks;
        std::optional<lockstep::anim::SkinnedModel> mercModel;
        bool isMercModelTried{false};
    };

    // Starts a presentation frame of NPC drawing.
    // looks: the looks.
    void beginNpcLooks(NpcLooks& looks);

    // Queues one armed NPC for drawing: the armed body (mercs in charcoal, head gear by
    // view.gearLook) playing the relaxed
    // rifle locomotion, or the aiming one while alert, squashed while crouched behind cover; a
    // dead one tips over backwards once and then lies still. A shot flashes a point light at
    // its muzzle. Without characters it is a box.
    // renderer:  the renderer.
    // looks:     the looks.
    // characters: the pawn characters (armed body and clips), or null.
    // visuals:   the fallback boxes.
    // archetype: Archetype::Merc or Archetype::Squad.
    // entity:    the local entity (key of its look).
    // transform: its interpolated transform (pawn-like: centre, yaw, capsule height as y scale).
    // view:      dead, alert, shots, head gear.
    // seconds:   presentation time since the last frame (World-scaled).
    void drawNpc(lockstep::render::Renderer& renderer, NpcLooks& looks, const PawnCharacters* characters,
        const Visuals& visuals, Archetype archetype, lockstep::world::EntityId entity,
        const lockstep::math::Transform& transform, const NpcView& view, float seconds);

    // Ends a presentation frame: forgets the looks of NPCs not drawn this frame.
    // looks: the looks.
    void endNpcLooks(NpcLooks& looks);
} //namespace game

#endif //LOCKSTEP_GAME_NPCS_NPC_PRESENTATION_HPP
