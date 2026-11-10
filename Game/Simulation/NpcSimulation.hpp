#ifndef LOCKSTEP_GAME_SIMULATION_NPC_SIMULATION_HPP
#define LOCKSTEP_GAME_SIMULATION_NPC_SIMULATION_HPP

#include <Lockstep/App/GameApi.hpp>
#include <Lockstep/Core/Math.hpp>
#include <Lockstep/Physics/PhysicsTypes.hpp>

#include <cstddef>
#include <optional>
#include <span>
#include <vector>

namespace game
{
    namespace math = lockstep::math;

    // Host: builds the navigation grid over a box of the level (the box around the level's NPC
    // markers) and logs its size.
    // state:   the game state (simulation.nav).
    // engine:  Engine services.
    // minimum, maximum: the box.
    void buildLevelNavigation(lockstep::app::GameState& state, lockstep::app::EngineContext& engine,
        const math::Vec3& minimum, const math::Vec3& maximum);

    // Host: removes every NPC and the navigation grid (level switch).
    // state:  the game state.
    // engine: Engine services.
    void clearLevelNpcs(lockstep::app::GameState& state, lockstep::app::EngineContext& engine);

    // Host: what a player's shot did to the NPCs: the shot is heard and suppresses NPCs it passes;
    // when it struck a merc's controller the merc takes NpcSettings::playerDamage (squad officers
    // take no friendly fire) and the shooter's hit and kill counters count it.
    // state:      the game state.
    // engine:     Engine services.
    // pawnIndex:  the shooter.
    // origin:     the shooter's eyes.
    // end:        where the shot stopped.
    // controller: the controller it struck, if any.
    // Returns:    true if it struck an NPC (merc or squad).
    bool shootNpcs(lockstep::app::GameState& state, lockstep::app::EngineContext& engine, std::size_t pawnIndex,
        const math::Vec3& origin, const math::Vec3& end, lockstep::physics::ControllerId controller);

    // Host: clears the shots heard last tick. Call at the start of the tick, before anyone fires.
    // state: the game state.
    void beginNpcTick(lockstep::app::GameState& state);

    // Host: runs every NPC for one tick at the World delta (groups, senses, decisions, movement,
    // fire), applies their hits to players (hurtPawn()) and NPCs, voices their callouts (the
    // host's subtitles and a message to the client) and sends the NPC state message on change.
    // state:  the game state.
    // engine: Engine services.
    void runNpcHostTick(lockstep::app::GameState& state, lockstep::app::EngineContext& engine);

    // Host: after a checkpoint restart: living mercs go back to their posts, unaware and healed;
    // the squad stands up again next to the players' checkpoint.
    // state:  the game state.
    // engine: Engine services.
    void restartNpcs(lockstep::app::GameState& state, lockstep::app::EngineContext& engine);

    // Host: the controllers of living NPCs, for the lag compensation record.
    // state:   the game state.
    // Returns: the controllers.
    [[nodiscard]] std::vector<lockstep::physics::ControllerId> npcControllers(const lockstep::app::GameState& state);

    // Client: takes a Game message if it is an NPC state or a callout (shown as a subtitle when
    // the speaker is within NpcSettings::calloutRange of the local pawn).
    // state:   the game state.
    // engine:  Engine services.
    // message: a received Game message.
    // Returns: true if it was an NPC message.
    bool receiveNpcMessage(lockstep::app::GameState& state, lockstep::app::EngineContext& engine,
        std::span<const std::byte> message);
} //namespace game

#endif //LOCKSTEP_GAME_SIMULATION_NPC_SIMULATION_HPP
