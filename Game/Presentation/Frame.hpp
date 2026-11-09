#ifndef LOCKSTEP_GAME_PRESENTATION_FRAME_HPP
#define LOCKSTEP_GAME_PRESENTATION_FRAME_HPP

#include <Lockstep/App/GameApi.hpp>

namespace game
{
    // Presents one rendered frame (the GameApi frame callback): Escape / click cursor capture, the
    // arena and level, Echo ghosts, lights, both pawns (the local one only in mirrors), toys, the
    // first-person view model, scene characters, HUD, and the camera (first person, the lobby view
    // while there is no local pawn, or the dev fly camera). Reads simulation state, never changes it.
    // state:     the game state.
    // engine:    Engine services.
    // frameInfo: interpolation alpha, frame time and input of this frame.
    void presentFrame(lockstep::app::GameState& state, lockstep::app::EngineContext& engine,
        const lockstep::app::FrameInfo& frameInfo);
} //namespace game

#endif //LOCKSTEP_GAME_PRESENTATION_FRAME_HPP
