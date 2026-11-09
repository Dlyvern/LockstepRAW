#ifndef LOCKSTEP_APP_RUN_HPP
#define LOCKSTEP_APP_RUN_HPP

#include "Lockstep/App/GameApi.hpp"
#include "Lockstep/Core/Result.hpp"
#include "Lockstep/Net/SessionLaunch.hpp"
#include "Lockstep/Platform/Window.hpp"
#include "Lockstep/Render/Renderer.hpp"

#include <cstdint>

namespace lockstep::app
{
    // Settings for run().
    // window:           the OS window.
    // renderer:         GPU context settings.
    // tickRate:         fixed simulation ticks per second.
    // maxTicksPerFrame: most ticks run in one frame; time beyond that is dropped after a stall.
    // session:          whether the network session hosts or connects at startup (see
    //                   net::parseSessionLaunch() for the command-line form).
    struct RunDescription
    {
        platform::WindowDescription window{};
        render::RendererDescription renderer{};
        double tickRate{60.0};
        uint32_t maxTicksPerFrame{5};
        net::SessionLaunch session{};
    };

    // Creates the window, renderer, dev tools, network session, time domains, entity store and
    // replication, then runs Game until the window closes. Each frame: poll input, run as many
    // fixed ticks as wall time allows (each advancing the time domains, moving every entity's
    // current transform to its previous one, calling GameApi::fixedTick, then sending
    // replication, pumping the network session and applying what arrived), draw
    // dev panels, call GameApi::frame with
    // the interpolation alpha, then render. While the dev tools want the keyboard or mouse, Game
    // sees that part of the input as idle (except the mouse while the cursor is captured).
    // description: window, renderer and tick settings.
    // game:        Game's callbacks.
    // Returns:     success when the window was closed, or the first error from Engine
    //              creation, starting the session, GameApi::init or rendering.
    [[nodiscard]] Status run(const RunDescription& description, const GameApi& game);
} //namespace lockstep::app

#endif //LOCKSTEP_APP_RUN_HPP
