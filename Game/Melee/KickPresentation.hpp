#ifndef LOCKSTEP_GAME_MELEE_KICK_PRESENTATION_HPP
#define LOCKSTEP_GAME_MELEE_KICK_PRESENTATION_HPP

#include "Melee/KickEvents.hpp"

#include <Lockstep/App/GameApi.hpp>
#include <Lockstep/World/Entities.hpp>

#include <span>
#include <vector>

namespace game
{
    // How a kick moves this machine's presentation physics, editable from the dev panel.
    // ragdollImpulse: impulse on the struck body of a ragdoll, kg m/s, plus ragdollLift upwards.
    // ragdollLift:    see ragdollImpulse.
    // pieceSpeed:     speed a severed limb, head or dropped weapon flies off with, m/s, plus
    //                 pieceLift upwards.
    // pieceLift:      see pieceSpeed.
    // reach:          how far from the kick's origin bodies and pieces are struck, metres.
    // reactionDamage: how hard a living NPC's procedural reaction is (as a hit of this damage).
    // windowSeconds:  how long a kick keeps pushing bodies that appear (a body that dies from it
    //                 ragdolls a frame later).
    struct KickPresentationTuning
    {
        float ragdollImpulse{240.0f};
        float ragdollLift{70.0f};
        float pieceSpeed{7.0f};
        float pieceLift{2.5f};
        float reach{1.4f};
        float reactionDamage{70.0f};
        float windowSeconds{0.3f};
    };

    // One kick being shown.
    // event:   the kick.
    // seconds: presentation seconds since it arrived.
    // pushed:  local entities whose body it already pushed or made flinch.
    // isShown: its pieces were pushed and its thud played.
    struct ShownKick
    {
        KickEvent event{};
        float seconds{0.0f};
        std::vector<lockstep::world::EntityId> pushed;
        bool isShown{false};
    };

    // Kicks as this machine shows them. Presentation only.
    // tuning:  the look.
    // pending: kicks to show next frame (the host's own, or received).
    // active:  kicks shown now.
    struct KickEffects
    {
        KickPresentationTuning tuning{};
        std::vector<KickEvent> pending;
        std::vector<ShownKick> active;
    };

    // Shows kicks: pushes the ragdolls of dead NPCs, severed limbs and heads, and dropped weapons
    // in reach, makes living NPCs in reach flinch hard and plays the thud of a kick that struck
    // something. Call after the NPCs are drawn this frame.
    // state:   the game state.
    // engine:  Engine services.
    // seconds: World time since the previous frame.
    void presentKicks(lockstep::app::GameState& state, lockstep::app::EngineContext& engine, float seconds);

    // Client: takes a Game message if it is a kick event and queues it to be shown.
    // effects: the kick effects.
    // message: a received Game message.
    // Returns: true if it was a kick event.
    bool receiveKickMessage(KickEffects& effects, std::span<const std::byte> message);

#if LOCKSTEP_DEV_TOOLS
    // Draws the kick presentation controls of the Game dev panel.
    // effects: the kick effects.
    void kickEffectsDevSection(KickEffects& effects);
#endif
} //namespace game

#endif //LOCKSTEP_GAME_MELEE_KICK_PRESENTATION_HPP
