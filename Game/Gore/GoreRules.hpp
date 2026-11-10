#ifndef LOCKSTEP_GAME_GORE_GORE_RULES_HPP
#define LOCKSTEP_GAME_GORE_GORE_RULES_HPP

#include "Gore/GoreEvents.hpp"

#include <Lockstep/Anim/Limbs.hpp>
#include <Lockstep/App/GameApi.hpp>
#include <Lockstep/Core/Math.hpp>
#include <Lockstep/World/Entities.hpp>

#include <cstddef>
#include <cstdint>

namespace game
{
    namespace math = lockstep::math;

    // A bullet hit on an NPC, as the host saw it.
    // npc:       index into NpcRoster::npcs.
    // point:     world point the bullet struck.
    // direction: unit direction it travelled.
    // damage:    hit points it took.
    // killed:    the hit killed the NPC.
    // isNpcShot: fired by another NPC (GoreRules::npcSevers decides if it may sever).
    // shooter:   the pawn index of the player who fired (k_noShooter for NPC shots); that
    //            player's machine already played the hit's impact sound.
    // distance:  metres from the shooter to the hit (0 if unknown); close shots burst heads more.
    struct NpcGoreHit
    {
        std::size_t npc{0};
        math::Vec3 point{0.0f, 0.0f, 0.0f};
        math::Vec3 direction{0.0f, 0.0f, -1.0f};
        int32_t damage{0};
        bool killed{false};
        bool isNpcShot{false};
        uint8_t shooter{k_noShooter};
        float distance{0.0f};
    };

    // Host: records the blood of a hit on an NPC and decides by GoreRules whether it severs the
    // limb it struck (found from the NPC's drawn pose: the bone nearest the hit point). A sever
    // replaces the plain hit event; a head sever may be a head burst instead (GoreRules). A killing hit remembers the limb (Npc::hitLimb) so both
    // machines push that part of the falling body.
    // state:   the game state (rules, NPC looks, this tick's events).
    // hit:     the hit.
    // Returns: the limb it struck (None if unknown), e.g. for the NPC's flinch.
    lockstep::anim::Limb recordNpcGore(lockstep::app::GameState& state, const NpcGoreHit& hit);

    // Host: records the blood of a hit on a player or any other character (no severing).
    // state:     the game state.
    // target:    the character's entity.
    // point:     world point of the hit.
    // direction: unit direction the shot travelled.
    // damage:    hit points it took (0 if unknown).
    void recordHitGore(lockstep::app::GameState& state, lockstep::world::EntityId target, const math::Vec3& point,
        const math::Vec3& direction, int32_t damage = 0);

    // Host: queues a severed limb directly (dev tools), shown on both machines.
    // state:  the game state.
    // target: the NPC's entity.
    // limb:   the limb.
    // point:  world point the blood comes from.
    void recordSever(lockstep::app::GameState& state, lockstep::world::EntityId target, lockstep::anim::Limb limb,
        const math::Vec3& point);

    // Reports a player's bullet that hit a dead body on this machine: the host queues it for its
    // next tick, the client sends it to the host.
    // state:  the game state.
    // engine: Engine services.
    // report: the hit.
    void queueCorpseHit(lockstep::app::GameState& state, lockstep::app::EngineContext& engine,
        const CorpseHitReport& report);

    // Host, during the tick: turns the queued and received corpse hits into gore events (a hit, or
    // a sever by the same GoreRules as a killing hit with the players' damage). Hits on anything
    // but a dead NPC are dropped.
    // state:  the game state.
    // engine: Engine services (received messages).
    void applyCorpseHits(lockstep::app::GameState& state, lockstep::app::EngineContext& engine);

    // Host, end of tick: sends this tick's gore events to the client and queues them for this
    // machine's next frame.
    // state:  the game state.
    // engine: Engine services.
    void publishGoreEvents(lockstep::app::GameState& state, lockstep::app::EngineContext& engine);
} //namespace game

#endif //LOCKSTEP_GAME_GORE_GORE_RULES_HPP
