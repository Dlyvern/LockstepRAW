#ifndef LOCKSTEP_GAME_GAME_HPP
#define LOCKSTEP_GAME_GAME_HPP

#include <Lockstep/App/GameApi.hpp>

namespace game
{
    // Returns: the table of Game callbacks the Engine run loop calls.
    [[nodiscard]] lockstep::app::GameApi gameApi();
} //namespace game

#endif //LOCKSTEP_GAME_GAME_HPP
