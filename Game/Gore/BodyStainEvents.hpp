#ifndef LOCKSTEP_GAME_GORE_BODY_STAIN_EVENTS_HPP
#define LOCKSTEP_GAME_GORE_BODY_STAIN_EVENTS_HPP

#include "Gore/GoreEvents.hpp"

#include <Lockstep/World/Entities.hpp>

#include <optional>

namespace lockstep::app
{
    struct EngineContext;
    struct GameState;
} //namespace lockstep::app

namespace game
{
    // Leaves a gore event's blood on the body it struck (CharacterStains): a stain at the hit
    // point, a bigger one at a stump or a burst neck. NPCs and other players' pawns; the local
    // player's own body shows its stains in mirrors.
    // state:  the game state (NPC looks, pawn characters, stain tuning).
    // engine: Engine services (the entity's owner for pawns).
    // entity: the struck character's local entity, if this machine knows it.
    // event:  the gore event.
    void stainStruckBody(lockstep::app::GameState& state, const lockstep::app::EngineContext& engine,
        std::optional<lockstep::world::EntityId> entity, const GoreEvent& event);

    // Dries blood, soaks bodies standing in the rain and queues every character's surface for
    // this frame (the NPC looks', the pawns' and the local arms'). Call after the weather and
    // before any character is drawn.
    // state:   the game state.
    // engine:  Engine services (renderer, entities, local player).
    // seconds: World seconds since the last frame.
    void prepareCharacterStains(lockstep::app::GameState& state, lockstep::app::EngineContext& engine, float seconds);
} //namespace game

#endif //LOCKSTEP_GAME_GORE_BODY_STAIN_EVENTS_HPP
